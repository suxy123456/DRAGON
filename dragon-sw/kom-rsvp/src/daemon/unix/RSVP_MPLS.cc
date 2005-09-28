
#include "RSVP_MPLS.h"
#include "RSVP_Log.h"
#include "RSVP_OIatPSB.h"
#include "RSVP_OutISB.h"
#include "RSVP_PSB.h"
#include "RSVP_PHopSB.h"
#include "RSVP_Session.h"
#include "SNMP_Global.h"
#if MPLS_REAL
#include "RSVP_RoutingService.h"
#include <linux/mpls.h>
#include <sys/ioctl.h>
#endif

#if defined(MPLS_WISCONSIN)
#include <asm/types.h>
#include <linux/rtnetlink.h>

extern "C" {
int rtnl_open();
int rtnl_recvfrom(int fd);
int send_nhlfe(int netlink,struct mpls_out_label_req *mol_req,int cmd);
int send_ilm(int netlink,struct mpls_in_label_req *mil_req,int cmd);
int send_ftn(int netlink,struct mpls_bind_fec_req *mbf_req,int cmd);
int send_xc(int netlink,struct mpls_xconnect_req *mx_req,int cmd);
int send_instr(int netlink,struct mpls_instruction_req *mir_req,int cmd);
}

#elif defined(MPLS_CAMBRIDGE)
#include <linux/mpls.h>
#include <net/if_arp.h>                           // struct arpreq

extern "C" {
int mpls_init(void);
void mpls_cleanup(void);
int mpls_add_switch_mapping(switch_mapping_t *sm);
int mpls_del_switch_mapping(cid_t *cid);
int mpls_add_port_mapping(port_mapping_t *pm);
int mpls_del_port_mapping(int port);
int mpls_add_ingress_mapping(ingress_mapping_t *im);
int mpls_del_ingress_mapping(fec_t *fec);
int mpls_add_egress_mapping(egress_mapping_t *em);
int mpls_del_egress_mapping(cid_t *cid);
int mpls_flush_all(void);
int mpls_debug_on(void); 
int mpls_debug_off(void);
}
#endif

const uint32 MPLS::filterHashSize = 1024;
const uint32 MPLS::minLabel = 16;
const uint32 MPLS::maxLabel = (1 << 20) - 1;

MPLS::MPLS( uint32 num, uint32 begin, uint32 end )
	: labelSpaceNum(num), labelSpaceBegin(begin), labelSpaceEnd(end), currentLabel(0),
	numberOfAllocatedLabels(0), labelHash(NULL),
	ingressClassifiers(filterHashSize) {

	if ( labelSpaceBegin < minLabel ) labelSpaceBegin = minLabel;
	if ( !labelSpaceEnd) labelSpaceEnd = maxLabel;
	currentLabel = labelSpaceBegin - 1;
}

bool MPLS::init() {
	labelHash = new uint32[RSVP_Global::labelHashCount];
	initMemoryWithZero( labelHash, sizeof(uint32)*RSVP_Global::labelHashCount );
#if defined(MPLS_WISCONSIN)
	netlink = CHECK( rtnl_open() );
	int fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	uint32 i;
	for ( i = 0; i < RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
		const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByLIH(i);
		if ( lif && !lif->isDisabled() && lif->hasEnabledMPLS() && lif->getSysIndex() != -1 ) {
			static struct mpls_labelspace_req mls_req;
			initMemoryWithZero( &mls_req, sizeof(mls_req) );
			mls_req.mls_ifindex = lif->getSysIndex();
			mls_req.mls_labelspace = labelSpaceNum;
			CHECK( ioctl( fd, SIOCSLABELSPACEMPLS, &mls_req ) );
		}
	}
	CHECK( close( fd ) );
#elif defined(MPLS_CAMBRIDGE)
	CHECK( mpls_init() );
	static port_mapping_t pm;
	initMemoryWithZero( &pm, sizeof(pm) );
	pm.type = LOCAL_PORT;
	pm.id = 0;
	CHECK( mpls_add_port_mapping( &pm ) );
#endif
	return true;
}

MPLS::~MPLS() {

#if MPLS_REAL
	uint32 x = 0;
	for ( ; x < filterHashSize; ++x ) {
		SortableHash<MPLS_Classifier*>::HashBucket::ConstIterator iter = ingressClassifiers[x].begin();
		for ( ; iter != ingressClassifiers[x].end(); ++iter ) {
			LOG(2)( Log::MPLS, "MPLS: cleaning up routing entry to", (*iter)->destAddress );
			RSVP_Global::rsvp->getRoutingService().delUnicastRoute( (*iter)->destAddress, NULL, NetAddress(0) );
			delete (*iter);
		}
	}
#endif

#if defined(MPLS_WISCONSIN)
	int fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	uint32 i;
	for ( i = 0; i < RSVP_Global::rsvp->getInterfaceCount(); ++i ) {
		const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByLIH(i);
		if ( lif && !lif->isDisabled() && lif->hasEnabledMPLS() && lif->getSysIndex() != -1 ) {
			static struct mpls_labelspace_req mls_req;
			initMemoryWithZero( &mls_req, sizeof(mls_req) );
			mls_req.mls_ifindex = lif->getSysIndex();
			mls_req.mls_labelspace = -1;
			CHECK( ioctl( fd, SIOCSLABELSPACEMPLS, &mls_req ) );
		}
	}
	close( fd );
	close( netlink );
#elif defined(MPLS_CAMBRIDGE)
	mpls_del_port_mapping( 0 );
	mpls_flush_all();
	mpls_cleanup();
#endif

	if (labelHash) delete [] labelHash;
}

inline uint32 MPLS::allocateInLabel() {
	if ( numberOfAllocatedLabels >= RSVP_Global::labelHashCount ) {
		FATAL(4)( Log::Fatal, "FATAL ERROR: number of labels", numberOfAllocatedLabels, "has exceeded the hash container size", RSVP_Global::labelHashCount );
		abortProcess();
	}
	do {
		currentLabel += 1;
		if ( currentLabel > labelSpaceEnd ) currentLabel = labelSpaceBegin;
	} while ( labelHash[currentLabel%RSVP_Global::labelHashCount] != 0 );
	LOG(2)( Log::MPLS, "MPLS: allocated label", currentLabel );
	numberOfAllocatedLabels += 1;
	return currentLabel;
	//return (uint32)1089538;  //port 1-1-10-2, just for current test, since we don't have any LMP nor do we have LabelSet from Movaz RE!
}

inline void MPLS::freeInLabel( uint32 label ) {
	LOG(2)( Log::MPLS, "MPLS: freeing label", label );
	numberOfAllocatedLabels -= 1;
	labelHash[label%RSVP_Global::labelHashCount] = 0;
}

inline MPLS_Classifier* MPLS::internCreateClassifier( const SESSION_Object& session, const SENDER_Object& sender, uint32 handle ) {
	LOG(3)( Log::MPLS, "MPLS: setting filter for", session, sender );
	MPLS_Classifier* filter = new MPLS_Classifier( session.getDestAddress() );
	SortableHash<MPLS_Classifier*>::HashBucket::Iterator iter = ingressClassifiers.lower_bound( filter ); 
	if ( iter != ingressClassifiers.getHashBucket( filter ).end() && **iter == *filter ) {
		delete filter;
		(*iter)->refCount += 1;
	} else {
		LOG(3)( Log::MPLS, "MPLS: creating filter for", session, sender );
		iter = ingressClassifiers.insert( iter, filter );
#if MPLS_REAL
		LOG(2)( Log::MPLS, "MPLS: creating routing entry to", (*iter)->destAddress );
		RSVP_Global::rsvp->getRoutingService().addUnicastRoute( session.getDestAddress(), NULL, NetAddress(0), handle );
#endif
	}
	return *iter;
}

inline void MPLS::internDeleteClassifier( const MPLS_Classifier* f ) {
	const_cast<MPLS_Classifier*>(f)->refCount -= 1;
	if ( f->refCount == 0 ) {
#if MPLS_REAL
		LOG(2)( Log::MPLS, "MPLS: removing routing entry to", f->destAddress );
		RSVP_Global::rsvp->getRoutingService().delUnicastRoute( f->destAddress, NULL, NetAddress(0) );
#endif
		LOG(2)( Log::MPLS, "MPLS: removing filter to", f->destAddress );
		ingressClassifiers.erase_key( const_cast<MPLS_Classifier*>(f) );
		delete f;
	}
}

void MPLS::handleOutLabel( OIatPSB& oiatpsb, uint32 label, const Hop& nhop ) {
	LOG(4)( Log::MPLS, "MPLS: storing output label", label, "for", oiatpsb );
	MPLS_OutLabel* outLabel = new MPLS_OutLabel( label );
#if defined(MPLS_WISCONSIN)
	outLabel->handle = nhop.getLogicalInterface().getSysIndex();
	static struct mpls_out_label_req mol_req;
	initMemoryWithZero( &mol_req, sizeof(mol_req) );
	mol_req.mol_label.ml_type = MPLS_LABEL_GEN;
	mol_req.mol_label.u.ml_gen = outLabel->getLabel();
	mol_req.mol_label.ml_index = outLabel->getLifSysIndex();
	reinterpret_cast<sockaddr_in&>(mol_req.mol_nh).sin_family = AF_INET;
	reinterpret_cast<sockaddr_in&>(mol_req.mol_nh).sin_addr.s_addr = nhop.getAddress().rawAddress();
	CHECK( send_nhlfe( netlink, &mol_req, RTM_NEWNHLFE ) );
#elif defined(MPLS_CAMBRIDGE)
	outLabel->handle = nhop.getHopInfoMPLS();
#endif
	outLabel->setLabelCType(oiatpsb.getRequestedOutLabelType());
	oiatpsb.setOutLabel( outLabel );
}

const MPLS_InLabel* MPLS::setInLabel( PSB& psb ) {

	MPLS_InLabel* inLabel;
	if (psb.getLABEL_SET_Object())
		inLabel = new MPLS_InLabel((const_cast<LABEL_SET_Object*>(psb.getLABEL_SET_Object()))->getASubChannel());
	else if (psb.hasSUGGESTED_LABEL_Object())
		inLabel = new MPLS_InLabel(psb.getSUGGESTED_LABEL_Object().getLabel());
	else if (psb.hasUPSTREAM_OUT_LABEL_Object()) //Make the best guess, just set the return label to be the same as the upstream label
		inLabel = new MPLS_InLabel(psb.getUPSTREAM_OUT_LABEL_Object().getLabel());
	else
		inLabel = new MPLS_InLabel( allocateInLabel() );
	inLabel->setLabelCType(psb.getLABEL_REQUEST_Object().getRequestedLabelType());
		
#if defined(MPLS_WISCONSIN)
	static struct mpls_in_label_req mil_req;
	initMemoryWithZero( &mil_req, sizeof(mil_req) );
	mil_req.mil_label.ml_type = MPLS_LABEL_GEN;
	mil_req.mil_label.u.ml_gen = inLabel->getLabel();
	mil_req.mil_label.ml_index = labelSpaceNum;
	CHECK( send_ilm( netlink, &mil_req, RTM_NEWILM ) );
#elif defined(MPLS_CAMBRIDGE)
	inLabel->handle = psb.getPHopSB().getHop().getHopInfoMPLS();
#endif
	return inLabel;
}

bool MPLS::bindInAndOut( PSB& psb, const MPLS_InLabel& il, const MPLS_OutLabel& ol, const MPLS* inLabelSpace ) {
	if ( !inLabelSpace ) inLabelSpace = this;
	LOG(6)( Log::MPLS, "MPLS: binding outgoing label", ol.getLabel(), "to input label", il.getLabel(), "from label space", inLabelSpace->labelSpaceNum );
#if defined(MPLS_WISCONSIN)
	static struct mpls_xconnect_req mx_req;
	initMemoryWithZero( &mx_req, sizeof(mx_req) );
	mx_req.mx_in.ml_type = MPLS_LABEL_GEN;
	mx_req.mx_in.u.ml_gen = il.getLabel();
	mx_req.mx_in.ml_index = inLabelSpace->labelSpaceNum;
	mx_req.mx_out.ml_type = MPLS_LABEL_GEN;
	mx_req.mx_out.u.ml_gen = ol.getLabel();
	mx_req.mx_out.ml_index = ol.getLifSysIndex();
	CHECK( send_xc( netlink, &mx_req, RTM_NEWXC ) );
#elif defined(MPLS_CAMBRIDGE)
	static switch_mapping_t sm;
	initMemoryWithZero( &sm, sizeof(sm) );
	sm.in_cid.port = il.getPort();
	sm.in_cid.label = il.getLabel();
	sm.out_cid.port = ol.getPort();
	sm.out_cid.label = ol.getLabel();
	CHECK( mpls_add_switch_mapping( &sm ) );
#endif
	if (!psb.getVLSR_Route().empty()){
		VLSRRoute::ConstIterator iter = psb.getVLSR_Route().begin();
		for ( ; iter != psb.getVLSR_Route().end(); ++iter ) {
			NetAddress ethSw = (*iter).switchID;
			SNMPSessionList::Iterator snmpIter = RSVP_Global::snmp->getSNMPSessionList().begin();
			bool noError = false;
			for (; snmpIter != RSVP_Global::snmp->getSNMPSessionList().end(); ++snmpIter ) {
				if ((*snmpIter)->getSwitchInetAddr()==ethSw && (*snmpIter)->isValidSession()){
					noError = true;
					uint32 vlan;
                                   if (((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL || ((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                    {
                                      vlan = (*iter).vlanTag;
                                    }
                                   else if (((*iter).inPort >> 16) == LOCAL_ID_TYPE_GROUP || ((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP)
                                    {
                                      vlan =   (*iter).inPort & 0xffff;
                                    }
                                   else if (((*iter).outPort >> 16) == LOCAL_ID_TYPE_GROUP || ((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP)
                                    {
                                      vlan =   (*iter).outPort & 0xffff;
                                    }
                                   else
                                        vlan = (*snmpIter)->findEmptyVLAN();

                                   if (!(*snmpIter)->verifyVLAN(vlan))
                                    {
        					LOG(4)( Log::MPLS, "VLSR: Cannot verify VLAN Tag(ID): ", vlan, " on Switch: ", ethSw);
                                    }

					if (vlan){
        					SimpleList<uint32> portList;
                                          uint32 port = (*iter).inPort;
                                          uint32 taggedPorts = 0;
                                          if ((port >> 16) == LOCAL_ID_TYPE_NONE)
                                              portList.push_back(port);
                                          else if ((port >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                              portList.push_back(port & 0xffff);
                                          else
                                              SNMP_Global::getPortsByLocalId(portList, port);
                                          if (portList.size() == 0){
        					      LOG(1)( Log::MPLS, "VLSR: Unrecognized port/localID at ingress.");
                                                return false;
                                          }
                                          while (portList.size()) {
                                                port = portList.front();
        						LOG(4)(Log::MPLS, "VLSR: Moving ingress port#",  port, " to VLAN #", vlan);
                                                 if (((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP || ((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL) {
                                                        (*snmpIter)->movePortToVLANAsTagged(port, vlan);
                                                        taggedPorts |= (1<<(32-port));
                                                    }
                                                 else
                                                    (*snmpIter)->movePortToVLANAsUntagged(port, vlan);
                                                portList.pop_front();
                                          }
                                          portList.clear();
                                          //taggedPorts = 0;
                                          port = (*iter).outPort;
                                          if ((port >> 16) == LOCAL_ID_TYPE_NONE)
                                              portList.push_back(port);
                                          else if ((port >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                              portList.push_back(port & 0xffff);
                                          else
                                              SNMP_Global::getPortsByLocalId(portList, port);
                                          if (portList.size() == 0){
        					      LOG(1)( Log::MPLS, "VLSR: Unrecognized port/localID at egress.");
                                                return false;
                                          }
                                          while (portList.size()) {
                                                port = portList.front();
        						LOG(4)(Log::MPLS, "VLSR: Moving egress port#",  port, " to VLAN #", vlan);
                                                 if (((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP || ((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL) {
                                                        (*snmpIter)->movePortToVLANAsTagged(port, vlan);
                                                        taggedPorts |= (1<<(32-port));
                                                    }
                                                 else
                                                        (*snmpIter)->movePortToVLANAsUntagged(port, vlan);
                                                portList.pop_front();
                                          }
                                          if (taggedPorts != 0)
                                            {
                                  		(*snmpIter)->setVLANPortsTagged(taggedPorts, vlan); //Set vlan ports to be "tagged"
							RSVP_Global::rsvp->getRoutingService().holdVtagbyOSPF(vlan, true); //true == hold
            					LOG(4)(Log::MPLS, "VLSR: Set tagged ports:",  taggedPorts, " in VLAN #", vlan);
                                            }
					}
					else{
						noError = false;
						LOG(2)( Log::MPLS, "VLSR: Cannot find an empty VLAN : ", ethSw);
					}
					break;
				}
			}
			if (!noError){
				return false;
			}
		}
	}

	return true;

}

void MPLS::createIngressClassifier( const SESSION_Object& session, const SENDER_Object& sender, const MPLS_OutLabel& ol ) {
	LOG(5)( Log::MPLS, "MPLS: creating ingress binding for", session, sender, "to label", ol.getLabel() );
	// create filter (i.e. routing entry)
	const_cast<MPLS_OutLabel&>(ol).filter = internCreateClassifier( session, sender, session.getDestAddress().rawAddress() );
#if defined(MPLS_WISCONSIN)
	// bind FEC to label
	static struct mpls_bind_fec_req mbf_req;
	initMemoryWithZero( &mbf_req, sizeof(mbf_req) );
	mbf_req.mbf_fec.prefix = session.getDestAddress().rawAddress();
	mbf_req.mbf_fec.len = 32;
	mbf_req.mbf_label.ml_type = MPLS_LABEL_GEN;
	mbf_req.mbf_label.u.ml_gen = ol.getLabel();
	mbf_req.mbf_label.ml_index = ol.getLifSysIndex();
	CHECK( send_ftn( netlink, &mbf_req, RTM_NEWFTN ) );
#elif defined(MPLS_CAMBRIDGE)
	static ingress_mapping_t im;
	initMemoryWithZero( &im, sizeof(im) );
	im.fec.proto = MPLSPROTO_IPV4;
	im.fec.u.ipv4.tclassid = session.getDestAddress().rawAddress();
	im.in_cid.port = 0;
	im.in_cid.label = session.getDestAddress().rawAddress();
	CHECK( mpls_add_ingress_mapping( &im ) );
	static switch_mapping_t sm;
	initMemoryWithZero( &sm, sizeof(sm) );
	sm.in_cid.port = 0;
	sm.in_cid.label = session.getDestAddress().rawAddress();
	sm.out_cid.port = ol.getPort();
	sm.out_cid.label = ol.getLabel();
	CHECK( mpls_add_switch_mapping( &sm ) );
#endif
}

void MPLS::createEgressBinding( const MPLS_InLabel& il, const LogicalInterface& lif ) {
	LOG(4)( Log::MPLS, "MPLS: creating egress binding for label", il.getLabel(), "to", lif.getName() );
#if defined(MPLS_CAMBRIDGE)
	static egress_mapping_t em;
	initMemoryWithZero( &em, sizeof(em) );
	em.in_cid.port = il.getPort();
	em.in_cid.label = il.getLabel();
	em.egress.proto = MPLSPROTO_IPV4;
	strcpy( em.egress.u.ipv4.ifname, lif.getName().chars() );
	CHECK( mpls_add_egress_mapping(&em) );
#endif
	const_cast<MPLS_InLabel&>(il).egressBinding = true;
}

bool MPLS::bindUpstreamInAndOut( PSB& psb) {
	LOG(4)( Log::MPLS, "MPLS: binding upstream outgoing label", psb.getUPSTREAM_OUT_LABEL_Object().getLabel(), 
					  "to input label", psb.getUPSTREAM_IN_LABEL_Object().getLabel());
	return true;
}

bool MPLS::createUpstreamIngressClassifier( const SESSION_Object& session, PSB& psb) {
	LOG(4)( Log::MPLS, "MPLS: creating upstream ingress binding for", session, "to input label", psb.getUPSTREAM_IN_LABEL_Object().getLabel());
	return true;
}

bool MPLS::createUpstreamEgressBinding( PSB& psb, const LogicalInterface& lif) {
	LOG(4)( Log::MPLS, "MPLS: creating upstream egress binding for label", psb.getUPSTREAM_OUT_LABEL_Object().getLabel(), "to", lif.getName());
	return true;
}

void MPLS::deleteInLabel(PSB& psb, const MPLS_InLabel* il ) {
	LOG(2)( Log::MPLS, "MPLS: deleting input label", il->getLabel() );
#if defined(MPLS_WISCONSIN)
	static struct mpls_in_label_req mil_req;
	initMemoryWithZero( &mil_req, sizeof(mil_req) );
	mil_req.mil_label.ml_type = MPLS_LABEL_GEN;
	mil_req.mil_label.u.ml_gen = il->getLabel();
	mil_req.mil_label.ml_index = labelSpaceNum;
	CHECK( send_ilm( netlink, &mil_req, RTM_DELILM ) );
#elif defined(MPLS_CAMBRIDGE)
	static cid_t cid;
	initMemoryWithZero( &cid, sizeof(cid) );
	cid.port = il->getPort();
	cid.label = il->getLabel();
	if ( il->egressBinding ) {
		CHECK( mpls_del_egress_mapping(&cid) );
	} else {
		CHECK( mpls_del_switch_mapping(&cid) );
	}
#endif
	freeInLabel( il->getLabel() );
	delete il;

	if (!psb.getVLSR_Route().empty()){
		VLSRRoute::ConstIterator iter = psb.getVLSR_Route().begin();
		for ( ; iter != psb.getVLSR_Route().end(); ++iter ) {
			NetAddress ethSw = (*iter).switchID;
			SNMPSessionList::Iterator snmpIter = RSVP_Global::snmp->getSNMPSessionList().begin();
			for (; snmpIter != RSVP_Global::snmp->getSNMPSessionList().end(); ++snmpIter ) {
				if ((*snmpIter)->getSwitchInetAddr()==ethSw && (*snmpIter)->isValidSession()){
    					SimpleList<uint32> portList;
                                      uint32 port = (*iter).inPort;
                                      if ((port >> 16) == LOCAL_ID_TYPE_NONE)
                                         portList.push_back(port);
                                      else if ((port >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                          portList.push_back(port & 0xffff);
                                      else
                                          SNMP_Global::getPortsByLocalId(portList, port);
                                      if (portList.size() == 0){
    					      LOG(1)( Log::MPLS, "VLSR: Unrecognized port/localID at ingress.");
                                            return;
                                      }
                                      while (portList.size()) {
                                            port = portList.front();
                                            if ((*iter).vlanTag != 0) {
       						LOG(4)(Log::MPLS, "VLSR: Removing ingress port#",  port, "from VLAN #", (*iter).vlanTag);
                                                (*snmpIter)->removePortFromVLAN(port, (*iter).vlanTag);
                                            }
                                            else {
        						LOG(3)(Log::MPLS, "VLSR: Moving ingress port#",  port, " back to Default VLAN #");
                                                (*snmpIter)->movePortToDefaultVLAN(port);
                                            }
                                            portList.pop_front();
                                      }
                                      portList.clear();
                                      port = (*iter).outPort;
                                      if ((port >> 16) == LOCAL_ID_TYPE_NONE)
                                         portList.push_back(port);
                                      else if ((port >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
                                          portList.push_back(port & 0xffff);
                                      else
                                          SNMP_Global::getPortsByLocalId(portList, port);
                                      if (portList.size() == 0){
    					      LOG(1)( Log::MPLS, "VLSR: Unrecognized port/localID at egress.");
                                            return;
                                      }
                                      while (portList.size()) {
                                            port = portList.front();
                                            if ((*iter).vlanTag != 0) {
                                                (*snmpIter)->removePortFromVLAN(port, (*iter).vlanTag);
       						LOG(4)(Log::MPLS, "VLSR: Removing egress port#",  port, "from VLAN #", (*iter).vlanTag);
                                            }
                                            else {
                                                  (*snmpIter)->movePortToDefaultVLAN(port);
    						        LOG(3)(Log::MPLS, "VLSR: Moving egress port#",  port, " back to Default VLAN #");
                                            }
                                            portList.pop_front();
                                      }
					if ((((*iter).inPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL 
					      || ((*iter).outPort >> 16) == LOCAL_ID_TYPE_TAGGED_GROUP_GLOBAL)
					  && (*iter).vlanTag != 0 && !(*snmpIter)->VLANHasTaggedPort((*iter).vlanTag))
					{
						RSVP_Global::rsvp->getRoutingService().holdVtagbyOSPF((*iter).vlanTag, false); //false == release
					}
					break;
                        }
                     }
			psb.getVLSR_Route().erase(iter);
		}
	}
}

void MPLS::deleteUpstreamInLabel(PSB& psb){
	LOG(2)( Log::MPLS, "MPLS: deleting upstream input label", psb.getUPSTREAM_IN_LABEL_Object().getLabel() );
}

void MPLS::deleteOutLabel( const MPLS_OutLabel* outLabel ) {
	LOG(2)( Log::MPLS, "MPLS: deleting output label", outLabel->getLabel() );
#if defined(MPLS_CAMBRIDGE)
	uint32 filterHandle = outLabel->filter ? outLabel->filter->destAddress.rawAddress() : 0;
#endif
	if ( outLabel->filter ) internDeleteClassifier( outLabel->filter );
#if defined(MPLS_WISCONSIN)
	static struct mpls_out_label_req mol_req;
	initMemoryWithZero( &mol_req, sizeof(mol_req) );
	mol_req.mol_label.ml_type = MPLS_LABEL_GEN;
	mol_req.mol_label.u.ml_gen = outLabel->getLabel();
	mol_req.mol_label.ml_index = outLabel->getLifSysIndex();
	CHECK( send_nhlfe( netlink, &mol_req, RTM_DELNHLFE ) );
#elif defined(MPLS_CAMBRIDGE)
	if (filterHandle) {
		static cid_t cid;
		initMemoryWithZero( &cid, sizeof(cid) );
		cid.port = 0;
		cid.label = filterHandle;
		CHECK( mpls_del_switch_mapping(&cid) );
		static fec_t fec;
		initMemoryWithZero( &fec, sizeof(fec) );
		fec.proto = MPLSPROTO_IPV4;
		fec.u.ipv4.tclassid = filterHandle;
		CHECK( mpls_del_ingress_mapping(&fec) );
	}
#endif
	delete outLabel;
}

void MPLS::deleteUpstreamOutLabel(PSB& psb){
	LOG(2)( Log::MPLS, "MPLS: deleting upstream output label", psb.getUPSTREAM_OUT_LABEL_Object().getLabel() );
}

#if defined(MPLS_CAMBRIDGE)
inline void MPLS::arpLookup( const LogicalInterface& lif, const NetAddress& addr, char r_addr[] ) {
	int fd = CHECK( socket( AF_INET, SOCK_DGRAM, 0 ) );
	static struct arpreq arpRequest;
	initMemoryWithZero( &arpRequest, sizeof(arpRequest) );
	reinterpret_cast<sockaddr_in&>(arpRequest.arp_pa).sin_family = AF_INET;
	reinterpret_cast<sockaddr_in&>(arpRequest.arp_pa).sin_port = 0;
	reinterpret_cast<sockaddr_in&>(arpRequest.arp_pa).sin_addr.s_addr = addr.rawAddress();
	strcpy( arpRequest.arp_dev, lif.getName().chars() );
	CHECK( ioctl( fd, SIOCGARP, &arpRequest ) );
	copyMemory( r_addr, arpRequest.arp_ha.sa_data, sizeof(arpRequest.arp_ha.sa_data) );
	close(fd);
}

uint32 MPLS::createHopInfo( const LogicalInterface& lif, const NetAddress& nhop ) {
	static int portnum = 0;
	static port_mapping_t pm;
	initMemoryWithZero( &pm, sizeof(pm) );
	pm.type = ETH_PORT;
	pm.id = ++portnum;
	strcpy( pm.u.eth.l_ifname, lif.getName().chars() );
	arpLookup( lif, nhop, pm.u.eth.r_addr );
	CHECK( mpls_add_port_mapping( &pm ) );
	return portnum;
}

void MPLS::removeHopInfo( uint32 port ) {
	CHECK( mpls_del_port_mapping( port ) );
}
#endif /* MPLS_CAMBRIDGE */

EXPLICIT_ROUTE_Object* MPLS::updateExplicitRoute( const NetAddress& dest, EXPLICIT_ROUTE_Object* er ) {
	if ( er ) er->borrow();
	ExplicitRouteList::ConstIterator erIter = erList.find( dest );
	if ( erIter != erList.end() ) {
                                           assert( !(*erIter).anList.empty() );
		if (!er) er = new EXPLICIT_ROUTE_Object;
		SimpleList<NetAddress>::ConstIterator addrIter = (*erIter).anList.end();
		for ( ;; ) {
			--addrIter;
			er->pushFront( AbstractNode( false, (*addrIter), (uint8)32 ) );
			if ( addrIter == (*erIter).anList.begin() ) {
		break;
			}
		}
	}
	if (er) {
		LOG(4)( Log::MPLS, "MPLS: explicit route for", dest, "is", *er );
	}
	return er;
}

void MPLS::addExplicitRoute( const NetAddress& dest, const SimpleList<NetAddress>& alist ) {
	ExplicitRoute er(dest);
	er.anList = alist;
	LOG(2)( Log::MPLS, "MPLS: setting explicit route", er );
	erList.insert_sorted( er );
}

ostream& operator<<( ostream& os, const ExplicitRoute& er ) {
	os << "dest: " << (NetAddress&)er << " via";
	SimpleList<NetAddress>::ConstIterator iter = er.anList.begin();
	for ( ; iter != er.anList.end(); ++iter ) {
		os << " " << *iter;
	}
	return os;
}

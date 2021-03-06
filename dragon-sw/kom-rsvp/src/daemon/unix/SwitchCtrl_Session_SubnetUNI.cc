/****************************************************************************

UNI Based Switch/Subnet Control Module source file SwitchCtrl_Session_SubnetUNI.cc
Created by Xi Yang @ 01/9/2007
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_SubnetUNI.h"
#include "RSVP.h"
#include "RSVP_Global.h"
#include "RSVP_RoutingService.h"
#include "RSVP_NetworkServiceDaemon.h"
#include "RSVP_Message.h"
#include "RSVP_MessageProcessor.h"
#include "RSVP_SignalHandling.h"
#include "RSVP_PSB.h"
#include "RSVP_Log.h"

SwitchCtrl_Session_SubnetUNI_List* SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = NULL;


void SwitchCtrl_Session_SubnetUNI::internalInit ()
{
    active = false;
    snmp_enabled = false; 
    rfc2674_compatible = false; 
    snmpSessionHandle = NULL; 
    uniSessionId = NULL; 
    uniState = 0; //Message::initApi
    swVersion = "";
    ctagNum = 0;
    numGroups = 0;
    ptpCatUnit = CATUNIT_UNKNOWN;
    resourceHeld = false;
    isTunnelMode = false;
    memset(&DTL, 0, sizeof(DTL_Subobject));
}

void SwitchCtrl_Session_SubnetUNI::setSubnetUniData(SubnetUNI_Data& data, uint8 subuni_id, uint8 first_ts,
 uint16 tunnel_id, float bw, uint32 tna_ipv4, uint32 uni_cid_ipv4, uint32 uni_nid_ipv4, uint32 data_if,
 uint32 port, uint32 egress_label, uint32 upstream_label, uint8* node_name, uint8* cc_name, uint8* bitmask)
{
    memset(&data, 0, sizeof(SubnetUNI_Data));
    data.subnet_id = subuni_id;
    data.first_timeslot = first_ts;
    data.tunnel_id = tunnel_id; 
    data.ethernet_bw = bw; 
    data.tna_ipv4 = tna_ipv4;
    data.logical_port = port;
    data.egress_label = egress_label; 
    data.upstream_label = upstream_label;

    data.uni_cid_ipv4 = uni_cid_ipv4;
    data.uni_nid_ipv4 = uni_nid_ipv4;
    data.data_if_ipv4 = data_if;

    memcpy(data.node_name, node_name, NODE_NAME_LEN);
    memcpy(data.control_channel_name, cc_name, CTRL_CHAN_NAME_LEN);
    memcpy(data.timeslot_bitmask, bitmask, MAX_TIMESLOTS_NUM/8);
}       

void SwitchCtrl_Session_SubnetUNI::setSubnetUniSrc(SubnetUNI_Data& data)
{
	const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByName(String((char*)data.control_channel_name));
	data.uni_cid_ipv4 = lif ? lif->getLocalAddress().rawAddress() : data.tna_ipv4; //?
	//uint32 data_if = 0;

	setSubnetUniData(subnetUniSrc, data.subnet_id, data.first_timeslot, data.tunnel_id, data.ethernet_bw, data.tna_ipv4, 
		data.uni_cid_ipv4, data.uni_nid_ipv4, data.data_if_ipv4, data.logical_port, data.egress_label, data.upstream_label, 
		data.node_name, data.control_channel_name, data.timeslot_bitmask);
}

void SwitchCtrl_Session_SubnetUNI::setSubnetUniDest(SubnetUNI_Data& data)
{
	const LogicalInterface* lif = RSVP_Global::rsvp->findInterfaceByName(String((char*)data.control_channel_name));
	data.uni_cid_ipv4 = lif ? lif->getLocalAddress().rawAddress() : data.tna_ipv4; //?
	//uint32 data_if = 0;

	setSubnetUniData(subnetUniDest, data.subnet_id, data.first_timeslot, data.tunnel_id, data.ethernet_bw, data.tna_ipv4, 
		data.uni_cid_ipv4, data.uni_nid_ipv4, data.data_if_ipv4, data.logical_port, data.egress_label, data.upstream_label, 
		data.node_name, data.control_channel_name, data.timeslot_bitmask);
}

const LogicalInterface* SwitchCtrl_Session_SubnetUNI::getControlInterface(NetAddress& gwAddress)
{
	SubnetUNI_Data* uniData = (isSource ? &subnetUniSrc : &subnetUniDest);
	const NetAddress nidAddress(uniData->uni_nid_ipv4);
	const LogicalInterface* lif = NULL;
	if ( uniData->control_channel_name[0] == 0 || strcmp((char*)uniData->control_channel_name, "implicit") == 0 )
	{
		lif = RSVP_Global::rsvp->getRoutingService().getUnicastRoute( nidAddress, gwAddress );
		gwAddress = nidAddress;
		return lif;
	}
	else
	{
		lif = RSVP_Global::rsvp->findInterfaceByName(String((char*)uniData->control_channel_name));
		if (lif)
			RSVP_Global::rsvp->getRoutingService().getPeerIPAddr(lif->getLocalAddress(), gwAddress);
		return lif;
	}
}

uint32 SwitchCtrl_Session_SubnetUNI::getPseudoSwitchID()
{
	SubnetUNI_Data* uniData = (isSource ? &subnetUniSrc : &subnetUniDest);
	//return (((uint32)uniData->subnet_id) << 16) | ((uint32)uniData->tunnel_id);
	//return ((uint32)uniData->tunnel_id);
	return ((((long)this)&0xff)*100 + (uint32)uniData->tunnel_id); //the session has an unique ID on this VLSR!
}

SwitchCtrl_Session_SubnetUNI::~SwitchCtrl_Session_SubnetUNI() 
{
    deregisterRsvpApiClient();
    if (uniSessionId)
        delete uniSessionId;
}

void SwitchCtrl_Session_SubnetUNI::uniRsvpSrcUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData)
{
    //should never be called
}

void SwitchCtrl_Session_SubnetUNI::uniRsvpDestUpcall(const GenericUpcallParameter& upcallParam, void* uniClientData)
{
    //should never be called
}


void SwitchCtrl_Session_SubnetUNI::registerRsvpApiClient()
{
    //insert the UNI ApiClient logicalLif into rsvp lifList if it has not been added.
    assert (RSVP_API::apiLif);
    assert (getFileDesc() > 0);
    const String ifName(RSVP_Global::apiUniClientName);
    if (!RSVP_Global::rsvp->findInterfaceByName(String(RSVP_Global::apiUniClientName))) {
        RSVP_Global::rsvp->addApiClientInterface(RSVP_API::apiLif);
    }
    NetworkServiceDaemon::registerApiClient_Handle(getFileDesc());
    if (!SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList)
        SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = new SwitchCtrl_Session_SubnetUNI_List;
    subnetUniApiClientList->push_front(this);
}

void SwitchCtrl_Session_SubnetUNI::deregisterRsvpApiClient()
{
    if(!SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList)
	return;
    SwitchCtrl_Session_SubnetUNI_List::Iterator it = subnetUniApiClientList->begin() ;
    for (; it != subnetUniApiClientList->end(); ++it) {
        if ((*it) == this) {
            subnetUniApiClientList->erase(it);
            break;
        }
    }
    if (subnetUniApiClientList->size() == 0) {
        NetworkServiceDaemon::deregisterApiClient_Handle(getFileDesc());
        delete SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList;
        SwitchCtrl_Session_SubnetUNI::subnetUniApiClientList = NULL;
    }
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessMessage(const Message& msg)
{
    //checking msg owner
    if (!isSessionOwner(msg))
        return;

    if (isSource)
        receiveAndProcessResv(msg);
    else
        receiveAndProcessPath(msg);
}

bool SwitchCtrl_Session_SubnetUNI::isSessionOwner(const Message& msg)
{
    const SESSION_Object* session_obj = &msg.getSESSION_Object();
    const LSP_TUNNEL_IPv4_SENDER_TEMPLATE_Object* sender_obj;
    const FlowDescriptorList* fdList;
    SubnetUNI_Data* uniData;

    if (isSource) {
        if ( !(session_obj->getDestAddress().rawAddress() == subnetUniSrc.uni_nid_ipv4 && session_obj->getTunnelId() == subnetUniSrc.tunnel_id) )
            return false;
        uniData = &subnetUniSrc;
    }
    else if ( !(session_obj->getDestAddress().rawAddress() == subnetUniDest.uni_cid_ipv4 && session_obj->getTunnelId() == subnetUniDest.tunnel_id) )
    {
        return false;
        uniData = &subnetUniDest;
    }

    if (msg.getMsgType() == Message::Path)
    {
        sender_obj = &msg.getSENDER_TEMPLATE_Object();
        if (sender_obj->getSrcAddress().rawAddress() == uniData->uni_cid_ipv4
            || sender_obj->getSrcAddress().rawAddress() == 0 || sender_obj->getSrcAddress().rawAddress() == 0x100007f)
            return true;
    }
    else if  (msg.getMsgType() == Message::Resv)
    {
        fdList = &msg.getFlowDescriptorList();
        FlowDescriptorList::ConstIterator it = fdList->begin();
        for (; it != fdList->end(); ++it)
        {
             if ((*it).filterSpecList.size()>0 && (*(*it).filterSpecList.begin()).getSrcAddress().rawAddress()  == uniData->uni_cid_ipv4)
                return true;
        }
    }

    return false;
}

void SwitchCtrl_Session_SubnetUNI::initUniRsvpApiSession()
{
    uniSessionId = new RSVP_API::SessionId();
    if (isSource)
        *uniSessionId = createSession( NetAddress(subnetUniSrc.uni_nid_ipv4), subnetUniSrc.tunnel_id,subnetUniSrc.uni_cid_ipv4, SwitchCtrl_Session_SubnetUNI::uniRsvpSrcUpcall);
    else
        *uniSessionId = createSession( NetAddress(subnetUniDest.uni_cid_ipv4), subnetUniDest.tunnel_id, subnetUniDest.uni_nid_ipv4, SwitchCtrl_Session_SubnetUNI::uniRsvpDestUpcall);
    active = true;

    uniState = Message::InitAPI;
}

static uint32 getSONETLabel(uint8 timeslot, SONET_TSpec* tspec)
{
    assert(tspec);
    switch (tspec->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        return (((uint32)(timeslot/3) << 16) | ((uint32)(timeslot%3) << 12) );
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        return ((uint32)(timeslot/3) << 16);
        break;
    }

    return 0;
}

void SwitchCtrl_Session_SubnetUNI::createRsvpUniPath()
{
    if (!active || !uniSessionId)
        initUniRsvpApiSession();
    if (!isSource)
    	return;

    SONET_SDH_SENDER_TSPEC_Object *stb = NULL;
    GENERALIZED_UNI_Object *uni = NULL;
    LABEL_SET_Object* labelSet = NULL;
    SESSION_ATTRIBUTE_Object* ssAttrib = NULL;
    UPSTREAM_LABEL_Object* upLabel = NULL;
    GENERALIZED_LABEL_REQUEST_Object *lr = NULL;

    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(subnetUniSrc.ethernet_bw);
    if (sonet_tb1)
        stb = new SENDER_TSPEC_Object(*sonet_tb1);

    // Pick the first available timeslot, convert it into SONET label and add to LABEL_SET
    uint8 ts;
    if (subnetUniSrc.first_timeslot == 0)
    {
        for (ts = 1; ts <= MAX_TIMESLOTS_NUM; ts++)
        {
            if ( HAS_TIMESLOT(subnetUniSrc.timeslot_bitmask, ts) )
            {
                subnetUniSrc.first_timeslot = ts;
                break;
            }
        }
    }
    assert (subnetUniSrc.first_timeslot != 0);
    labelSet = new LABEL_SET_Object(); //LABEL_GENERALIZED
    uint32 label = getSONETLabel(subnetUniSrc.first_timeslot, sonet_tb1); 
    labelSet->addSubChannel(htonl(label));
    subnetUniSrc.upstream_label = htonl(label);

    // Update egress_label ... (using the same upstream label) ==> always do symetric provisioning!
    if (subnetUniDest.first_timeslot == 0)
    {
        for (ts = 1; ts <= MAX_TIMESLOTS_NUM; ts++)
        {
            if ( HAS_TIMESLOT(subnetUniDest.timeslot_bitmask, ts) )
            {
                subnetUniDest.first_timeslot = ts;
                break;
            }
        }
    }
    assert (subnetUniDest.first_timeslot != 0);
    label = getSONETLabel(subnetUniDest.first_timeslot, sonet_tb1);
    subnetUniDest.egress_label = subnetUniDest.upstream_label = htonl(label);

    uni = new GENERALIZED_UNI_Object (subnetUniSrc.tna_ipv4, subnetUniDest.tna_ipv4, 
                    subnetUniDest.logical_port, subnetUniDest.egress_label,
                    subnetUniDest.logical_port, subnetUniDest.upstream_label);

    ssAttrib = new SESSION_ATTRIBUTE_Object(sessionName);

    upLabel = new UPSTREAM_LABEL_Object(subnetUniSrc.upstream_label);

    lr = new LABEL_REQUEST_Object ( LABEL_REQUEST_Object::L_ANSI_SDH, 
    							       LABEL_REQUEST_Object::S_TDM,
                                                        LABEL_REQUEST_Object::G_SONET_SDH);

    //NetAddress srcAddress(subnetUniSrc.uni_cid_ipv4);
    //createSender( *uniSessionId, srcAddress, subnetUniSrc.tunnel_id, *stb, *lr, NULL, uni, labelSet, ssAttrib, upLabel, 50);
    createSender( *uniSessionId, subnetUniSrc.tunnel_id, *stb, *lr, NULL, uni, NULL, labelSet, ssAttrib, upLabel, 50); //dagonExtInfo?

    if (uni) uni->destroy();
    if (labelSet) labelSet->destroy();
    if (lr) delete lr;
    if (stb) delete stb;
    if (ssAttrib) delete ssAttrib;
    if (upLabel) delete upLabel;

    if (uniState == Message::InitAPI)
        uniState = Message::Path;

    return;
}

void SwitchCtrl_Session_SubnetUNI::createRsvpUniResv(const SONET_SDH_SENDER_TSPEC_Object& sendTSpec, const LSP_TUNNEL_IPv4_FILTER_SPEC_Object& senderTemplate)
{
    if (!active || !uniSessionId)
        return;

    SONET_SDH_FLOWSPEC_Object* flowspec = new FLOWSPEC_Object((const SONET_TSpec&)(sendTSpec));
    FlowDescriptorList fdList;
    fdList.push_back( flowspec );
    fdList.back().filterSpecList.push_back( senderTemplate );
    //IPv4_IF_ID_RSVP_HOP_Object? ==> revise RSVP_AP::createReservation

    createReservation( *uniSessionId, false, FF, fdList, NULL);

    if (uniState == Message::InitAPI)
        uniState = Message::Resv;

    return;
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessPath(const Message & msg)
{
    if (!active)
        return;

    //change session states ...
    //notify main session indirectly
    uniState = msg.getMsgType();

    switch (msg.getMsgType())
    {
    case Message::Path:
    case Message::PathResv:

        createRsvpUniResv(msg.getSENDER_TSPEC_Object(), msg.getSENDER_TEMPLATE_Object());

        break;
    case Message::InitAPI:

        assert( *(*uniSessionId) );
        //refreshSession( **(*uniSessionId), RSVP_Global::defaultApiRefresh );

        break;
    default:
        //unprocessed RSVP messages
        break;
    }

    return;
}

void SwitchCtrl_Session_SubnetUNI::receiveAndProcessResv(const Message & msg)
{
    if (!active)
        return;

    //change session states ...
    //notify main session indirectly
    uniState = msg.getMsgType();
    
    switch (msg.getMsgType())
    {
    case Message::Resv:
    case Message::ResvConf:

        break;
    case Message::PathErr:

        break;
    case Message::ResvTear:

        break;
    case Message::InitAPI:

        assert( *(*uniSessionId) );
        //refreshSession( **(*uniSessionId), RSVP_Global::defaultApiRefresh );

	break;
    default:
        break;
    }

    return;
}

void SwitchCtrl_Session_SubnetUNI::releaseRsvpPath()
{
    if (!active || !uniSessionId)
        return;

    assert( *(*uniSessionId) );
    releaseSession(**(*uniSessionId));

    if (isSource)
        uniState = Message::PathTear;
    else
        uniState = Message::ResvTear; 

    return;
}

void SwitchCtrl_Session_SubnetUNI::refreshUniRsvpSession()
{
    if (!active)
        return;
    
    //do nothing?
}

void SwitchCtrl_Session_SubnetUNI::getTimeslots(SimpleList<uint8>& timeslots)
{
    timeslots.clear();
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;

    if (ptpCatUnit == CATUNIT_UNKNOWN)
    {
    	if ((ptpCatUnit = getConcatenationUnit_TL1()) == CATUNIT_UNKNOWN)
            return;
    }
    uint8 ts = pUniData->first_timeslot;
    if (ptpCatUnit == CATUNIT_150MBPS && ts%3 != 1)
        return;

    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    if (!sonet_tb1)
        return;

    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    //Handling both contiguous and non-contiguous modes
    //As first_timeslot has been calculated by NARB or syncTimslots(), no more calculation here.
    for (uint8 x = 0; x < ts_num && ts <= MAX_TIMESLOTS_NUM; ts++)
    {
    	if (HAS_TIMESLOT(pUniData->timeslot_bitmask, ts))
   		{
	        timeslots.push_back(ts);
			x++;
   		}
    }
}

//////////////////////////////////
/////// TL1 related commands  //////
/////////////////////////////////

String& SwitchCtrl_Session_SubnetUNI::getCienaSoftwareVersion()
{
    if (swVersion.empty())
        getCienaSoftwareVersion_TL1(swVersion);
    return swVersion;
}

void SwitchCtrl_Session_SubnetUNI::getCienaTimeslotsString(String& groupMemString)
{
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;
    uint8 ts = pUniData->first_timeslot;

    if (ptpCatUnit == CATUNIT_UNKNOWN)
    {
        if ((ptpCatUnit = getConcatenationUnit_TL1()) == CATUNIT_UNKNOWN)
    	{
            groupMemString = "";
            return;
    	}
    }
    if (ptpCatUnit == CATUNIT_150MBPS && ts%3 != 1)
    {
        groupMemString = "";
        return;
    }


    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    if (!sonet_tb1)
    {
        groupMemString = "";
        return;
    }

    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    if (ts_num == 0 || ts+ts_num-1 > MAX_TIMESLOTS_NUM)
    {
        groupMemString = "";
        return;
    }

	
	if ((pUniData->options & IFSWCAP_SPECIFIC_SUBNET_CONTIGUOUS) == 0)
	{
	    sprintf(bufCmd, "%d", ts);
		++ts;
		int ts_count = 1;
		char sts[8];
		for (; ts_count < ts_num && ts <= MAX_TIMESLOTS_NUM; ts++)
		{
			if (HAS_TIMESLOT(pUniData->timeslot_bitmask, ts))
			{

				sprintf(sts, "&%d", ts);
				strcat(bufCmd, sts);
				ts_count++;
			}
		}
		if (ts_count < ts_num)
		{
			groupMemString = "";
			return;
		}
			
	}
	else
	{
	    sprintf(bufCmd, "%d&&%d", ts, ts+ts_num-1);
	}
    groupMemString = (const char*)bufCmd;
}

void SwitchCtrl_Session_SubnetUNI::getCienaCTPGroupsInVCG(String*& ctpGroupStringArray, String& vcgName)
{
    assert(ctpGroupStringArray);
    int group;
    for (group = 0; group < 4; group++) 
        ctpGroupStringArray[group] = "";
    
    char ctp[60];
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;
    uint8 ts = pUniData->first_timeslot;

    if (ptpCatUnit == CATUNIT_UNKNOWN)
    {
    	if ((ptpCatUnit = getConcatenationUnit_TL1()) == CATUNIT_UNKNOWN)
    	{
            return;
    	}
    }
    if (ptpCatUnit == CATUNIT_150MBPS && ts%3 != 1)
    {
        return;
    }

    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    if (!sonet_tb1)
    {
        return;
    }

    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    if (ts_num == 0 || ts+ts_num-1 > MAX_TIMESLOTS_NUM)
    {
        return;
    }

    uint8 first_ts = ts;
    group = 0; 
    numGroups = 0;
    int ts_offset = 0;
    if (ptpCatUnit == CATUNIT_150MBPS)
    {
        sprintf(bufCmd, "%s-CTP-%d", vcgName.chars(), ts/3+1);
        ts_num += ts;
        ts += 3;
        for ( ; ts < ts_num + ts_offset && ts <= MAX_TIMESLOTS_NUM; ts += 3)
        {
            if ((pUniData->options & IFSWCAP_SPECIFIC_SUBNET_CONTIGUOUS) == 0 && !HAS_TIMESLOT(pUniData->timeslot_bitmask, ts))
            {
			ts_offset += 3;
			continue;
            }
            if (ts - first_ts - ts_offset == 48)
            {
                ctpGroupStringArray[group] = (const char*)bufCmd;
                group++;
                numGroups++;
                first_ts = ts;
                ts_num += ts_offset;
                ts_offset = 0;
                sprintf(bufCmd, "%s-CTP-%d", vcgName.chars(), ts/3+1);
                continue;
            }
            sprintf(ctp, "&%s-CTP-%d", vcgName.chars(), ts/3+1);
            strcat(bufCmd, ctp);
        }
        ctpGroupStringArray[group] = (const char*)bufCmd;
        numGroups++;        
    }
    else // must be CATUNIT_50MBPS
    {
        assert (ptpCatUnit == CATUNIT_50MBPS);
        sprintf(bufCmd, "%s-CTP-%d", vcgName.chars(), ts);
        ts_num += ts;
        ts += 1;
        for ( ; ts < ts_num + ts_offset && ts <= MAX_TIMESLOTS_NUM; ts++)
        {
            if ((pUniData->options & IFSWCAP_SPECIFIC_SUBNET_CONTIGUOUS) == 0 && !HAS_TIMESLOT(pUniData->timeslot_bitmask, ts))
            {
			ts_offset ++;
			continue;
            }
            if (ts - first_ts - ts_offset == 48)
            {
                ctpGroupStringArray[group] = (const char*)bufCmd;
                group++;
                numGroups++;
                first_ts = ts;
                ts_num += ts_offset;
                ts_offset = 0;
                sprintf(bufCmd, "%s-CTP-%d", vcgName.chars(), ts);
                continue;
            }
            sprintf(ctp, "&%s-CTP-%d", vcgName.chars(), ts);
            strcat(bufCmd, ctp);
        }
        ctpGroupStringArray[group] = (const char*)bufCmd;
        numGroups++;        
    }

}


void SwitchCtrl_Session_SubnetUNI::getCienaLogicalPortString(String& OMPortString, String& ETTPString, uint32 logicalPort)
{
    int bay, shelf, slot, subslot, port;
    char shelf_alpha;
    SubnetUNI_Data* pSubnetUni = (isSource ? &subnetUniSrc : &subnetUniDest);

    if (logicalPort == 0)
    {
        logicalPort = ntohl(pSubnetUni->logical_port);
    }

    bay = (logicalPort >> 24) + 1;
    shelf = ((logicalPort >> 16)&0xff);
    slot = ((logicalPort >> 12)&0x0f) + 1;
    subslot = ((logicalPort >> 8)&0x0f) + 1;
    port = (logicalPort&0xff) + 1;

    switch (shelf)
    {
    case 2:
        shelf_alpha = 'A';
        break;
    case 3:
        shelf_alpha = 'C';
        break;
    default:
        shelf_alpha = 'X';
        break;
    }
    String &swVrsn = getCienaSoftwareVersion();
    if (swVrsn.leftequal("5."))
        sprintf(bufCmd, "%d-%c-%d-%d", bay, shelf_alpha, slot, subslot);
    else //CD v6 or higher
        sprintf(bufCmd, "%d-%c-%d-1", bay, shelf_alpha, slot);
    OMPortString = (const char*)bufCmd;
    sprintf(bufCmd, "%d-%c-%d-%d-%d", bay, shelf_alpha, slot, subslot, port);
    ETTPString = (const char*)bufCmd;
}

void SwitchCtrl_Session_SubnetUNI::getCienaDestTimeslotsString(String*& destTimeslotsStringArray)
{
    assert(destTimeslotsStringArray);
    int group;
    for (group = 0; group < 4; group++) 
        destTimeslotsStringArray[group] = "";

    int bay, shelf, slot, subslot;
    char shelf_alpha;
    uint32 logicalPort = ntohl(subnetUniDest.logical_port);
    uint8 ts = subnetUniDest.first_timeslot;

    if (ptpCatUnit == CATUNIT_UNKNOWN)
    {
    	if ((ptpCatUnit = getConcatenationUnit_TL1()) == CATUNIT_UNKNOWN)
    	{
            return;
    	}
    }
    if (ptpCatUnit == CATUNIT_150MBPS && ts%3 != 1)
    {
        return;
    }

    bay = (logicalPort >> 24) + 1;
    shelf = ((logicalPort >> 16)&0xff);
    slot = ((logicalPort >> 12)&0x0f) + 1;
    subslot = ((logicalPort >> 8)&0x0f) + 1;

    switch (shelf)
    {
    case 2:
        shelf_alpha = 'A';
        break;
    case 3:
        shelf_alpha = 'C';
        break;
    default:
        shelf_alpha = 'X';
        break;
    }

    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(subnetUniDest.ethernet_bw);
    if (!sonet_tb1)
    {
        return;
    }

    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }
    if (ts_num == 0 || ts+ts_num-1 > MAX_TIMESLOTS_NUM || ts_num/48 != numGroups - (ts_num%48 == 0 ? 0 : 1))
    {
        return;
    }

    int ts_count = 0;
    for (group = 0; group < numGroups; group++)
    {
	if ((subnetUniDest.options & IFSWCAP_SPECIFIC_SUBNET_CONTIGUOUS) == 0)
	{
		sprintf(bufCmd, "%d-%c-%d-%d-%d", bay, shelf_alpha, slot, subslot, ts);
		++ts;
		++ts_count;
		char sts[8];
		for (; ts_count < ts_num && ts <= MAX_TIMESLOTS_NUM; ts++)
		{
			if (HAS_TIMESLOT(subnetUniDest.timeslot_bitmask, ts))
			{
				sprintf(sts, "&%d", ts);
				strcat(bufCmd, sts);
				ts_count++;
				if (ts_count%48 == 0) 
				{
					ts++;
					break;
				}
			}
		}				
		destTimeslotsStringArray[group] = (const char*)bufCmd;
	}
	else 
	{
	    sprintf(bufCmd, "%d-%c-%d-%d-%d&&%d", bay, shelf_alpha, slot, subslot, ts+group*48, (group == numGroups-1? ts+ts_num-1 : ts+group*48+47));
	    destTimeslotsStringArray[group] = (const char*)bufCmd;
	}
    }
}

void SwitchCtrl_Session_SubnetUNI::getPeerCRS_GTP(String& gtpName)
{
    gtpName = "";
    SwitchCtrl_Session_SubnetUNI* pSubnetSession;
    
    SwitchCtrlSessionList::Iterator sessionIter = RSVP_Global::switchController->getSessionList().begin();
    for ( ; sessionIter != RSVP_Global::switchController->getSessionList().end(); ++sessionIter)
    {
        if ( (*sessionIter)->getSessionName().leftequal("subnet-uni") ) {
            pSubnetSession = (SwitchCtrl_Session_SubnetUNI*)(*sessionIter);
            if (pSubnetSession != this && this->isSourceClient() != pSubnetSession->isSourceClient() 
                && pSubnetSession->getSubnetUniDest()->subnet_id == this->getSubnetUniDest()->subnet_id
                && pSubnetSession->getSubnetUniDest()->tunnel_id == this->getSubnetUniDest()->tunnel_id)

            {
                pSubnetSession->getCurrentGTP(gtpName);
                break;
            }
        }
    }
    return;
}

void SwitchCtrl_Session_SubnetUNI::getDTLString(String& dtlStr)
{
    dtlStr = "";
    if (DTL.count == 0 || DTL.count > MAX_DTL_LEN)
        return;
    bufCmd[0] = 0;
    char hop[40];
    for (uint32 i=0; i < DTL.count; i++)
    {
        sprintf(hop, "nodename%d=%s,osrpltpid%d=%d,", i+1, (char*)DTL.hops[i].nodename, i+1, DTL.hops[i].linkid);
        strcat(bufCmd, hop);
    }
    sprintf(hop, "termnodename=%s", (char*)subnetUniDest.node_name);
    strcat(bufCmd, hop);
    dtlStr = bufCmd;
    return;
}

//rtrv-eqpt::com:234;
void SwitchCtrl_Session_SubnetUNI::getCienaSoftwareVersion_TL1(String &swVrsn)
{
    int ret = 0;
    
    sprintf(bufCmd, "rtrv-eqpt::com:%d;", getNewCtag());
    
    if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;
    
    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1)
    {
        ret = ReadShellPattern(bufCmd, (char*)",VRSN=", NULL, (char*)";", NULL, 5);
        if (ret != 1)
            goto _out;
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", "getCienaSoftwareVersion_TL1", " retrieved Core Director version number.\n", bufCmd);
        char* pVrsn = strstr(bufCmd, "VRSN="); 
        pVrsn += 5;
        *(pVrsn+7) = '\0';
        swVersion = pVrsn;
        return;
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", "getCienaSoftwareVersion_TL1", " failed to get Core Director version number.\n", bufCmd);
        swVersion = "";
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return;
    }
    else
        goto _out;

_out:
        LOG(5)(Log::MPLS, "LSP=", currentLspName, ": ", "getCienaSoftwareVersion_TL1 via TL1_TELNET failed...\n", bufCmd);
        swVersion = "";
        return;
}

//v5: ent-eflow::myeflow1:123:::ingressporttype=ettp,ingressportname=1-A-3-1-1, 
//v6: ent-eflow::myeflow1:123::ingressporttype=ettp,ingressportname=1-A-3-1-1, 
//pkttype=single_vlan_tag,outervlanidrange=1&&5,,priority=1&&8,egressporttype=vcg, 
//egressportname=vcg02,cosmapping=cos_port_default;
//
//                  $$$$ We do not consider inner VLAN tags at this point!
//
bool SwitchCtrl_Session_SubnetUNI::createEFLOWs_TL1(String& vcgName, int vlanLow, int vlanHigh, int vlanTrunk)
{
    int ret = 0;
    char colonPadding[10];
    char packetType[100];
    char modificationRule[100];
    String suppTtp, ettpName;


    String &swVrsn = getCienaSoftwareVersion();
    if (swVrsn.empty())
        goto _out;

    if (swVrsn.leftequal("5."))
        sprintf(colonPadding, ":::");
    else if (swVrsn.leftequal("6."))
        sprintf(colonPadding, "::");
    else
        goto _out;

    if (vlanLow == 0)
        sprintf(packetType, "pkttype=untagged_unicast,,");
    else if (vlanLow == ANY_VTAG)
        sprintf(packetType, "pkttype=all,,");
    else if (vlanHigh <= MAX_VLAN && vlanHigh > vlanLow)
        sprintf(packetType, "pkttype=single_vlan_tag,outervlanidrange=%d&&%d,,priority=1&&8", vlanLow,vlanHigh);
    else
        sprintf(packetType, "pkttype=single_vlan_tag,outervlanidrange=%d,,priority=1&&8", vlanLow);

    getCienaLogicalPortString(suppTtp, ettpName);

    // applying VLAN modification rule on ingress eflow
    if (vlanLow > 0 && vlanLow <= MAX_VLAN && vlanLow != vlanTrunk) 
    {
        sprintf(modificationRule, "tagstoremove=remove_outer,");
    }
    else
    {
        sprintf(modificationRule, "tagstoremove=remove_none,");
    }
    if (vlanTrunk > 0 && vlanTrunk <= MAX_VLAN && vlanLow != vlanTrunk) 
    {
        sprintf(modificationRule+strlen(modificationRule), "tagstoadd=add_outer,outertagtype=0X8100,outervlanid=%d,", vlanTrunk);
    }
    else
    {
        sprintf(modificationRule+strlen(modificationRule), "tagstoadd=add_none,");
    }

    sprintf(bufCmd, "ent-eflow::dcs_eflow_%s_in:%d%singressporttype=ettp,ingressportname=%s,%s,egressporttype=vcg,egressportname=%s,cosmapping=cos_port_default,%scollectpm=yes;",
        vcgName.chars(), getNewCtag(), colonPadding, ettpName.chars(), packetType, vcgName.chars(), modificationRule);

    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Ingress-EFLOW has been created successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        // contine to other EFLOW creation ...
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Ingress-EFLOW creation has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

    if (strncmp(packetType, "pkttype=untagged", 16) == 0)
    {
        sprintf(bufCmd, "ent-eflow::dcs_eflow_%s_in_multicast:%d%singressporttype=ettp,ingressportname=%s,pkttype=untagged_multicast,,,egressporttype=vcg,egressportname=%s,cosmapping=cos_port_default,%scollectpm=yes;",
            vcgName.chars(), getNewCtag(), colonPadding, ettpName.chars(), vcgName.chars(), modificationRule);
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
    
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Ingress-EFLOW (untagged multicast) has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            // contine to other EFLOW creation ...
        }
        else if (ret == 2)
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Ingress-EFLOW (untagged multicast) creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;

    }

    if (vlanTrunk == 0)
        sprintf(packetType, "pkttype=untagged_unicast,,");
    else if (vlanTrunk == ANY_VTAG)
        sprintf(packetType, "pkttype=all,,");
    else if (vlanTrunk <= MAX_VLAN)
        sprintf(packetType, "pkttype=single_vlan_tag,outervlanidrange=%d,,priority=1&&8", vlanTrunk);

    // applying VLAN modification rule on egress eflow
    if (vlanTrunk > 0 && vlanTrunk <= MAX_VLAN && vlanLow != vlanTrunk)
    {
        sprintf(modificationRule, "tagstoremove=remove_outer,");
    }
    else
    {
        sprintf(modificationRule, "tagstoremove=remove_none,");
    }
    if (vlanLow > 0 && vlanLow <= MAX_VLAN && vlanLow != vlanTrunk) 
    {
        sprintf(modificationRule+strlen(modificationRule), "tagstoadd=add_outer,outertagtype=0X8100,outervlanid=%d,", vlanLow);
    }
    else
    {
        sprintf(modificationRule+strlen(modificationRule), "tagstoadd=add_none,");
    }

    sprintf(bufCmd, "ent-eflow::dcs_eflow_%s_out:%d%singressporttype=vcg,ingressportname=%s,%s,egressporttype=ettp,egressportname=%s,cosmapping=cos_port_default,%scollectpm=yes;",
        vcgName.chars(), getNewCtag(), colonPadding, vcgName.chars(), packetType, ettpName.chars(), modificationRule);

    if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Egress-EFLOW has been created successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        if (strncmp(packetType, "pkttype=untagged", 16) != 0)
            return true;
        //otherwise, contine to create egress untagged multicast eflow
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Egress-EFLOW creation has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

    if (strncmp(packetType, "pkttype=untagged", 16) == 0)
    {
        sprintf(bufCmd, "ent-eflow::dcs_eflow_%s_out_multicast:%d%singressporttype=vcg,ingressportname=%s,pkttype=untagged_multicast,,,egressporttype=ettp,egressportname=%s,cosmapping=cos_port_default,%scollectpm=yes;",
            vcgName.chars(), getNewCtag(), colonPadding, vcgName.chars(), ettpName.chars(), modificationRule);
    
        if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;
    
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Egress-EFLOW (untagged multicast) has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            // done!
            return true;
        }
        else if (ret == 2)
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Egress-EFLOW (untagged multicast) creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;
    }

_out:
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " EFLOWs creation via TL1_TELNET failed...\n", bufCmd);
        return false;
}

//dlt-elow::myeflow1:myctag;
bool SwitchCtrl_Session_SubnetUNI::deleteEFLOWs_TL1(String& vcgName, bool hasIngressUntaggedMulticast, bool hasEgressUntaggedMulticast)
{
    int ret = 0;

    sprintf(bufCmd, "dlt-eflow::dcs_eflow_%s_in:%d;", vcgName.chars(), getNewCtag());

    if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Ingress-EFLOW has been deleted successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        // contine to delete other eflows
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Ingress-EFLOW deletion has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

    sprintf(bufCmd, "dlt-eflow::dcs_eflow_%s_out:%d;", vcgName.chars(), getNewCtag());

    if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Egress-EFLOW has been deleted successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        if (!hasIngressUntaggedMulticast&&!hasEgressUntaggedMulticast)
            return true;
        //otherwise continue to delete other eflows
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Egress-EFLOW deletion has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

    if (hasIngressUntaggedMulticast)
    {
        sprintf(bufCmd, "dlt-eflow::dcs_eflow_%s_in_multicast:%d;", vcgName.chars(), getNewCtag());
       
        if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;
       
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Ingress-EFLOW (untagged multicast) has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            // contine to egress untagged multicast EFLOW creation ...
        }
        else if (ret == 2)
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Ingress-EFLOW (untagged multicast) deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;
    }

    if (hasEgressUntaggedMulticast)
    {
        sprintf(bufCmd, "dlt-eflow::dcs_eflow_%s_out_multicast:%d;", vcgName.chars(), getNewCtag());
    
        if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;
    
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Egress-EFLOW (untagged multicast) has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return true;
        }
        else if (ret == 2)
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " Egress-EFLOW (untagged multicast) deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;
    }

_out:
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " EFLOWs deletion via TL1_TELNET failed...\n", bufCmd);
        return false;
}

//rtrv-eflow::myeflow1:myctag;
bool SwitchCtrl_Session_SubnetUNI::hasEFLOW_TL1(String& vcgName, bool ingress, bool untaggedMulticast)
{
    int ret = 0;

    sprintf(bufCmd, "rtrv-eflow::dcs_eflow_%s_%s%s:%d;", vcgName.chars(), ingress? "in":"out", untaggedMulticast? "_multicast":"", getNewCtag());

    if ( (ret = writeShell((char*)bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, (ingress? "_in":"_out"), (untaggedMulticast? "_multicast":"_unicast"), " EFLOW does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, (ingress? "_in":"_out"), (untaggedMulticast? "_multicast":"_unicast"), " EFLOW does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, (ingress? "_in":"_out"), " EFLOW existence checking via TL1_TELNET failed...\n", bufCmd);
        return false;    
}

//ENT-VCG::NAME=vcg01:456::,PST=is,SUPPTTP=1-A-3-1,CRCTYPE=CRC_32,,,FRAMINGMODE=GFP,
//TUNNELPEERTYPE=ETTP,TUNNELPEERNAME=1-A-3-1-1,,GFPFCSENABLED=yes,,,GROUPMEM=1&&3,,;
bool SwitchCtrl_Session_SubnetUNI::createVCG_TL1(String& vcgName, bool tunnelMode)
{
    int ret = 0;
    char ctag[10];
    String suppTtp, tunnelPeerName, groupMem;

    getCienaLogicalPortString(suppTtp, tunnelPeerName);
    getCienaTimeslotsString(groupMem);
    if ((groupMem).empty())
    {
        LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "getCienaTimeslotsString failed to find available time slots...");
        vcgName = "";
        return false;
    }

    sprintf(ctag, "%d", getNewCtag());
    vcgName = "dcs_vcg_";
    vcgName += (const char*)ctag;

    String cmdString = "ent-vcg::name=";
    cmdString += vcgName;
    cmdString += ":";
    cmdString += (const char*)ctag;
    cmdString += "::alias=";
    cmdString += currentLspName;
    cmdString += ",pst=IS,suppttp=";
    cmdString += suppTtp;
    if (tunnelMode)
    {
        isTunnelMode = true;
        cmdString += ",crctype=CRC_32,,,framingmode=GFP,tunnelpeertype=ETTP,tunnelpeername=";
        cmdString += tunnelPeerName;
    }
    else
    {
        cmdString += ",crctype=CRC_32,,,framingmode=GFP,tunnelpeertype=NONE,";
    }
    cmdString += ",,gfpfcsenabled=YES,,,groupmem=";
    cmdString += groupMem;
    cmdString += ",,;";

    if ( (ret = writeShell((char*)cmdString.chars(), 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " has been created successfully.\n", cmdString);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " creation has been denied.\n", cmdString);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        vcgName = "";
        return false;
    }
    else 
        goto _out;

_out:
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " creation via TL1_TELNET failed...\n", cmdString);
        vcgName = "";
        return false;
}

//ED-VCG::NAME=vcg01:123::,PST=OOS;
//DLT-VCG::NAME=vcg01:123;
bool SwitchCtrl_Session_SubnetUNI::deleteVCG_TL1(String& vcgName)
{
    int ret = 0;
    uint32 ctag = getNewCtag();

    sprintf(bufCmd, "ed-vcg::name=%s:%d::,pst=OOS;", vcgName.chars(), ctag);
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " status has been set to OOS.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
	//continue to next command ...
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " status change (to OOS) has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

    //sleep one second to let finish status change into OOS  
    sleep(1);

    ctag = getNewCtag();
    sprintf(bufCmd, "dlt-vcg::name=%s:%d;", vcgName.chars(), ctag);
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " has been deleted successfully.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " deletion has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " change/deletion via TL1_TELNET failed...\n", bufCmd);
        return false;
}

bool SwitchCtrl_Session_SubnetUNI::hasVCG_TL1(String& vcgName)
{
    int ret = 0;

    sprintf( bufCmd, "rtrv-vcg::%s:%d;\r", vcgName.chars(), getNewCtag() );
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " VCG does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, "TRUNCATED\"", true, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " VCG does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", vcgName, " VCG existence checking via TL1_TELNET failed...\n", bufCmd);
        return false;    
}

//;ENT-GTP::gtp1:123::lbl=label,,ctp=vcg01-CTP-1&vcg01-CTP-2&vcg01-CTP-3&vcg01-CTP-4;
bool SwitchCtrl_Session_SubnetUNI::createGTP_TL1(String& gtpName, String& vcgName)
{
    int ret = 0;
    char ctag[10];
    sprintf(ctag, "%d", getNewCtag());
    gtpName = "dcs_gtp_";
    gtpName += ctag;

    String ctpGroupStringArray[4];
    String* pString = ctpGroupStringArray;
    getCienaCTPGroupsInVCG(pString, vcgName);
    if (ctpGroupStringArray[0].empty() || numGroups == 0)
    {
        LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "getCienaCTPGroupsInVCG returned empty strings");
        gtpName = "";
        return false;
    }

    int group;
    for (group = 0; group < numGroups; group++)
    {
        assert(!ctpGroupStringArray[group].empty());

        sprintf( bufCmd, "ent-gtp::%s-%d:%s::lbl=gtp-%s,,ctp=%s;", gtpName.chars(), group+1, ctag, vcgName.chars(), ctpGroupStringArray[group].chars() );

        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", gtpName, "-", group+1, " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", gtpName, "-", group+1, " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            gtpName = "";
            return false;
        }
        else 
            goto _out;
    }

    return true;
    
_out:
    LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", gtpName, "-", group+1, " creation via TL1_TELNET failed...\n", bufCmd);
    gtpName = "";
    return false;    
}

//;DLT-GTP::gtp1:123;
bool SwitchCtrl_Session_SubnetUNI::deleteGTP_TL1(String& gtpName)
{
    int ret = 0;
    int group;
    for (group = 0; group < numGroups; group++)
    {
        sprintf( bufCmd, "dlt-gtp::%s-%d:%d;", gtpName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", gtpName, "-", group+1, " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", gtpName, "-", group+1, " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;
    }

    return true;

_out:
    LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", gtpName, "-", group+1, " deletion via TL1_TELNET failed...\n", bufCmd);
    return false;    
}

bool SwitchCtrl_Session_SubnetUNI::hasGTP_TL1(String& gtpName)
{
    int ret = 0;

    //only checking the first group if more than one.
    sprintf( bufCmd, "rtrv-gtp::%s-1:%d;", gtpName.chars(), getNewCtag() );
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", gtpName, " GTP does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", gtpName, " GTP does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
    LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", gtpName, " GTP existence checking via TL1_TELNET failed...\n", bufCmd);
    return false;    
}

//;ent-snc-stspc:SEAT:gtp_x,1-a-5-1-1&&21:myctag::name=sncname,type=dynamic,rmnode=GRNOC,lep=gtp_nametype,conndir=bi_direction,prtt=aps_vlsr_unprotected,pst=is;
bool SwitchCtrl_Session_SubnetUNI::createSNC_TL1(String& sncName, String& gtpName)
{
    int ret = 0;
    char ctag[10];

    sprintf(ctag, "%d", getNewCtag());
    sncName = "dcs_snc_";
    sncName += ctag;

    assert(numGroups > 0);
    // get destination time slots!
    String destTimeslotsStringArray[4];
    String* pString = destTimeslotsStringArray;
    getCienaDestTimeslotsString(pString);
    if (destTimeslotsStringArray[0].empty())
    {
        LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "getCienaDestTimeslotsString returned empty strings.");
        sncName = "";
        return false;
    }

    //creatign DTL and DTL-SET
    String dtlString;
    if (DTL.count > 0)
    {
        getDTLString(dtlString);
        if (dtlString.empty())
        {
            LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "getDTLString returned empty strings.");
            sncName = "";
            return false;
        }

        //ent-dtl::dtl1:123::NODENAME1=SEAT,OSRPLTPID1=1,TERMNODENAME=GRNOC;
        //DTL named 'sncname-dtl'
        sprintf( bufCmd, "ent-dtl::%s-dtl:%d::%s;", sncName.chars(), getNewCtag(), dtlString.chars());
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl", " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl", " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            sncName = "";
            return false;
            // OR continue to SNC creation with 'dtlexcl=no' option ?'
        }
        else 
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl", " creation via TL1_TELNET failed...\n", bufCmd);
            return false;
        }

        //ent-dtl-set::dtlset1:123::WRKNM=dtl1,;
        //DTL-SET named 'sncname-dtl_set'
        sprintf( bufCmd, "ent-dtl-set::%s-dtl_set:%d::wrknm=%s-dtl,;", sncName.chars(), getNewCtag(), sncName.chars());
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl_set", " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl_set", " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            sncName = "";
            // ? Delete the created DTL ?
            return false;
            // OR continue to SNC creation with 'dtlexcl=no' option ?'
        }
        else 
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl_set", " deletion via TL1_TELNET failed...\n", bufCmd);
            return false;
        }
    }

    int group;
    for (group = 0; group < numGroups; group++)
    {
        char dtl_cstr[40];
        char supptptype_cstr[10];
        dtl_cstr[0] = 0;
        if (!dtlString.empty())
        {
            sprintf(dtl_cstr, "dtlsn=%s-dtl_set, dtlexcl=yes,", sncName.chars());
        }
        supptptype_cstr[0] = 0;
        String &swVrsn = getCienaSoftwareVersion();
        if (swVrsn.leftequal("6.2"))
        {
            sprintf(supptptype_cstr,"supptptype=sttp,");
        }
        sprintf( bufCmd, "ent-snc-stspc:%s:%s-%d,%s:%s::name=%s-%d,type=dynamic,rmnode=%s,%slep=gtp_nametype,alias=%s,%sconndir=bi_direction,meshrst=no,prtt=aps_vlsr_unprotected,pst=is;",
            (const char*)subnetUniSrc.node_name, gtpName.chars(), group+1, destTimeslotsStringArray[group].chars(), ctag, sncName.chars(), group+1, 
                (const char*)subnetUniDest.node_name, supptptype_cstr, currentLspName.chars(), dtl_cstr);

        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %s COMPLD", ctag);
        sprintf(strDENY, "M  %s DENY", ctag);
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-", group+1, " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-", group+1, " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);

            // TODO: dlt-snc for other groups; dlt-dlt-set:: ; dlt-dlt::

            sncName = "";
            return false;
        }
        else 
            goto _out;
    }

    return true;

_out:
    LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-", group+1, " creation via TL1_TELNET failed...\n", bufCmd);
    sncName = "";
    return false;    
}

//ED-SNC-STSPC::snc001:123::,PST=OOS;
//dlt-snc-stspc::snc_2:myctag;
bool SwitchCtrl_Session_SubnetUNI::deleteSNC_TL1(String& sncName)
{
    int ret = 0;
    String dtlString;

    int group;
    for (group = 0; group < numGroups; group++)
    {
        sprintf( bufCmd, "ed-snc-stspc::%s-%d:%d::,pst=oos;", sncName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-", group+1, " state has been changed into OOS.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            //continue to next command ...
        }
        else if (ret == 2)
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-", group+1, " state change to OOS has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;

        // sleep 7 second to let finish status change into OOS  
        sleep(7);

        sprintf( bufCmd, "dlt-snc-stspc::%s-%d:%d;", sncName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-", group+1, " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-", group+1, " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            //continue to delete dtl-set
        }
        else 
            goto _out;
    }

    getDTLString(dtlString);
    if (!dtlString.empty())
    {
        //dlt-dtl-set::dtlset2:123;
        sprintf( bufCmd, "dlt-dtl-set::%s-dtl_set:%d;", sncName.chars(), getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl_set", " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl_set", " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            //continue to delete dtl
        }
        else 
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl_set", " deletion via TL1_TELNET failed...\n", bufCmd);
            return false;
        }
        
        //dlt-dtl::dtl1:123;
        sprintf( bufCmd, "dlt-dtl::%s-dtl:%d;", sncName.chars(), getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl", " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl", " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
        {
            LOG(7)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-dtl", " deletion via TL1_TELNET failed...\n", bufCmd);
            return false;
        }
    }
	
    return true;

_out:
    LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, "-", group+1, " change/deletion via TL1_TELNET failed...\n", bufCmd);
    return false;    
}

bool SwitchCtrl_Session_SubnetUNI::hasSNC_TL1(String& sncName)
{
    int ret = 0;

    //only checking the first SNC if more than one SNCs are created for the LSP.
    sprintf( bufCmd, "rtrv-snc-stspc::%s-1:%d;", sncName.chars(), getNewCtag() );
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, " SNC does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, " SNC does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
    LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, " SNC existence checking via TL1_TELNET failed...\n", bufCmd);
    return false;    
}

//return 0 if all snc's have been in stable working state 
//return negative id (-1 ~ -4) of the first group/snc that has an error
//return positive id (1 ~ 4) of the first group/snc that is in neither stable or erro state
int SwitchCtrl_Session_SubnetUNI::verifySNCInStableWorkingState_TL1(String& sncName)
{
    int funcRet = 0;
    int group;
    for (group = 0; group < numGroups; group++)
    {
        int ret = 0;
        sprintf( bufCmd, "rtrv-snc-diag::%s-%d:%d;", sncName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;
        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            ret = ReadShellPattern(bufCmd, (char*)"Snc IC Path Defect Clear", NULL, (char*)";", NULL, 5);
            if (ret == 1)
            {
                //making sure there is no 'Backoff Expiry' and 'STARTING' status after 'Snc IC Path Defect Clear'
                char* pMoreRecords = strstr(bufCmd, "Snc IC Path Defect Clear");
                if (strstr(pMoreRecords, "Backoff Expiry") != NULL || strstr(pMoreRecords, "STARTING") != NULL)
                    return -(group+1); //this SNC is in unstable/error state
            }
            else if (strstr(bufCmd, "Backoff Expiry") != NULL)
            {
                return -(group+1); //this SNC is not in unstatble/error state
            }
            else 
            {
                funcRet = group + 1; //this SNC is in neither working or unstatble/error state
                continue;
            }
        }
        else if (ret == 2)
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", "verifySNCWorkingStatus_TL1 found no such SNC:", sncName, '-', group,  "\n");
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return -(group+1);
        }
        else 
            goto _out;
    }

   //all snc's are in stable working state -> funcRet == 0; or one of the snc's not ready (neither working or error) -> funcRet > 0
   return funcRet;
   
_out:
    LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", sncName, " SNC existence checking via TL1_TELNET failed...\n", bufCmd);
    return -(numGroups+1);    
}


//;ent-crs-stspc::fromendpoint=gtp01,toendpoint=gtp02:myctag::name=crs01,fromtype=gtp,totype=gtp,;
bool SwitchCtrl_Session_SubnetUNI::createCRS_TL1(String& crsName, String& gtpName)
{
    int ret = 0;
    char ctag[10];

    sprintf(ctag, "%d", getNewCtag());
    crsName = "dcs_crs_";
    crsName += ctag;

    // get destination time slots!
    String destGtpName;
    getPeerCRS_GTP(destGtpName);
    if (destGtpName.empty())
    {
        LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "createCRS_TL1:getPeerCRS_GTP returned empty string.");
        crsName = "";
        return false;
    }

    int group;
    for (group = 0; group < numGroups; group++)
    {
        sprintf( bufCmd, "ent-crs-stspc::fromendpoint=%s-%d,toendpoint=%s-%d:%s::name=%s-%d,fromtype=gtp,totype=gtp, alias=%s;",
            gtpName.chars(), group+1, destGtpName.chars(), group+1, ctag, crsName.chars(), group+1, currentLspName.chars());

        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, "-", group+1, " has been created successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, "-", group+1, " creation has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            crsName = "";
            return false;
        }
        else 
            goto _out;
    }

    return true;

_out:
    LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, "-", group+1, " creation via TL1_TELNET failed...\n", bufCmd);
    crsName = "";
    return false;
}

bool SwitchCtrl_Session_SubnetUNI::deleteCRS_TL1(String& crsName)
{
    int ret = 0;

    int group;
    for (group = 0; group < numGroups; group++)
    {
        sprintf( bufCmd, "ed-crs-stspc::name=%s-%d:%d::,pst=oos;", crsName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, "-", group+1, " state has been changed into OOS.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            //continue to next command ...
        }
        else if (ret == 2)
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, "-", group+1, " state change to OOS has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;

        sprintf( bufCmd, "dlt-crs-stspc::name=%s-%d:%d;", crsName.chars(), group+1, getNewCtag() );
        if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

        sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
        sprintf(strDENY, "M  %d DENY", getCurrentCtag());
        ret = readShell(strCOMPLD, strDENY, 1, 5);
        if (ret == 1) 
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, "-", group+1, " has been deleted successfully.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
        }
        else if (ret == 2)
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, "-", group+1, " deletion has been denied.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else 
            goto _out;
    }

    //sleep one second to let finish status change for cross-connect 
    sleep(1);

    return true;

_out:
    LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, "-", group+1, " change/deletion via TL1_TELNET failed...\n", bufCmd);
    return false;
}

//rtrv-crs-stspc::name=crsName:123;
bool SwitchCtrl_Session_SubnetUNI::hasCRS_TL1(String& crsName)
{
    int ret = 0;

    //only checking the first CRS if more than one is created for the LSP.
    sprintf( bufCmd, "rtrv-crs-stspc::name=%s-1:%d;", crsName.chars(), getNewCtag() );
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, " XConn does exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, " XConn does not exist.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else 
        goto _out;

_out:
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", crsName, " Xconn existence checking via TL1_TELNET failed...\n", bufCmd);
        return false;    
}


//v5: rtrv-ocn::1-A-5-1:mytag;
//v6: rtrv-map::1-A-5-1:mytag;

SONET_CATUNIT SwitchCtrl_Session_SubnetUNI::getConcatenationUnit_TL1(uint32 logicalPort)
{
    int ret = 0;
    SONET_CATUNIT funcRet = CATUNIT_UNKNOWN;
    String OMPortString, ETTPString;

    String &swVrsn = getCienaSoftwareVersion();
    if (swVrsn.empty())
        goto _out;

    getCienaLogicalPortString(OMPortString, ETTPString, logicalPort);
    if (swVrsn.leftequal("5."))
        sprintf(bufCmd, "rtrv-ocn::%s:%d;", OMPortString.chars(), getNewCtag());
    else if (swVrsn.leftequal("6.")) 
        sprintf(bufCmd, "rtrv-map::%s:%d;", OMPortString.chars(), getNewCtag());
    else 
        goto _out;
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        if (swVrsn.leftequal("5."))
            ret = ReadShellPattern(bufCmd, (char*)"Virtual 50MBPS", (char*)"Virtual 150MBPS", (char*)"OSPFCOST", NULL, 5);
        else if(swVrsn.leftequal("6."))
            ret = ReadShellPattern(bufCmd, (char*)"VIRTUAL_50MBPS", (char*)"VIRTUAL_150MBPS", (char*)"MAXETHERFRAMESIZE", NULL, 5);
        else 
            goto _out;

        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": Get ", OMPortString, " concatenation type.\n", bufCmd);
        if (ret == 1)
            funcRet = CATUNIT_50MBPS;
        else if (ret == 2)
            funcRet = CATUNIT_150MBPS;

        readShell(SWITCH_PROMPT, NULL, 1, 5);
    }
    else if (ret == 2) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", OMPortString, " concatenation type checking has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return funcRet;
    }
    else
        goto _out;

_out:
    if (funcRet == CATUNIT_UNKNOWN)
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", OMPortString, " concatenation type checking via TL1_TELNET failed...\n", bufCmd);
    }
    return funcRet;
}

// Get timeslots map based on the allocated OCN timeslots on the card that has the logicalPort
// OCN timeslots are allocated when a cross connect is in place
bool SwitchCtrl_Session_SubnetUNI::syncTimeslotsMapOCN_TL1(uint8 *ts_bitmask, uint32 logicalPort)
{
    int ret = 0;
    String OMPortString, ETTPString;
    char* pstr;
    int ts;

    assert(ts_bitmask);
    //@@@@ Should not set all bits. Otherwise, the timeslot configuration in ospfd.conf will be overriden.
    //memset(ts_bitmask, 0xff, MAX_TIMESLOTS_NUM/8);

    getCienaLogicalPortString(OMPortString, ETTPString, logicalPort);

    sprintf(bufCmd, "rtrv-ocn::%s:%d;", OMPortString.chars(), getNewCtag());
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", OMPortString, " syncTimeslotsMapOCN_TL1 method has retrieved timeslots suceessfully.\n", bufCmd);
        ret = ReadShellPattern(bufCmd, NULL, NULL, (char*)",NOACT", NULL, 5);
        if (ret == 0) {
            bufCmd[strlen(bufCmd) - 6] = 0;
            pstr = strstr(bufCmd, "TIMESLOTMAP=");
            if (!pstr)
                goto _out;
            pstr = strtok(pstr+12, " &");
            while (pstr)
            {
                if (sscanf(pstr, "%d", &ts) == 1)
                    RESET_TIMESLOT(ts_bitmask, ts);
                pstr = strtok(NULL, " &");
            }
        }
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return true;
    }
    else if (ret == 2) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", OMPortString, " syncTimeslotsMapOCN_TL1 retrieving timeslots has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else
        goto _out;

_out:
    LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", OMPortString, " syncTimeslotsMapOCN_TL1 method via TL1_TELNET failed...\n", bufCmd);
    return false;
}

// Get timeslots map based on existing VCGs on the card that has the logicalPort.
// This is more acturate timeslots map to avoid conflicting timeslots allocation to simultaneous VCGs.
bool SwitchCtrl_Session_SubnetUNI::syncTimeslotsMapVCG_TL1(uint8 *ts_bitmask, uint32 logicalPort)
{
    int ret = 0;
    String OMPortString, ETTPString;
    char* pstr;
    int ts, ts1, ts2;

    assert(ts_bitmask);
    //@@@@ Should not set all bits. Otherwise, the timeslot configuration in ospfd.conf will be overriden.
    //memset(ts_bitmask, 0xff, MAX_TIMESLOTS_NUM/8);

    getCienaLogicalPortString(OMPortString, ETTPString, logicalPort);

    sprintf(bufCmd, "rtrv-vcg::all:%d;\r", getNewCtag());
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        ret = readShell("   /* Empty", "   \"", 1, 5);
        if (ret == 1)
        {
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return true;
        }
        else if (ret == 2)
        {
            while ((ret = ReadShellPattern(bufCmd, (char*)OMPortString.chars(), (char*)"GROUPMEM=", (char*)"VCGFAILUREBASESEV=", (char*)";", 5)) != READ_STOP)
            { // if (ret == 3), we have reach the end, i.e., ";"...
                if (ret == 1)
                {
                    pstr = strstr(bufCmd, "GROUPMEM=");
                    if (!pstr)
                        goto _out;
                    ret = sscanf(pstr+9, "%d&&%d", &ts1, &ts2);
                    if (ret == 1)
                        ts2 = ts1;
                    else if (ret <= 0)
                        goto _out;
                    for (ts = ts1; ts <= ts2; ts++)
                    {
                        RESET_TIMESLOT(ts_bitmask, ts);
                    }
                }
                else if (ret == 2)
                    continue; // not one of the VCGs we are looing for
                else
                    goto _out; // wrong
            }
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", OMPortString, " syncTimeslotsMapVCG_TL1 method has retrieved timeslots suceessfully.\n", bufCmd);
            return true;    
        }
        else
            goto _out;
    }
    else if (ret == 2) 
    {
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", OMPortString, " syncTimeslotsMapVCG_TL1 retrieving timeslots has been denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else
        goto _out;

_out:
    LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", OMPortString, " syncTimeslotsMapVCG_TL1 method via TL1_TELNET failed...\n", bufCmd);
    return false;
}

bool SwitchCtrl_Session_SubnetUNI::syncTimeslotsMap() 
{
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;
    bool ret = syncTimeslotsMapVCG_TL1(pUniData->timeslot_bitmask);

    //TODO: make this an inline function...
    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    assert (sonet_tb1);
    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    if (ret)
    {
        uint8 ts, ts_count;
        bool ts_ok = false;
        for (ts = 1; ts <= MAX_TIMESLOTS_NUM; ts++)
        {				
            if (HAS_TIMESLOT(pUniData->timeslot_bitmask, ts))
            {
				if ((pUniData->options & IFSWCAP_SPECIFIC_SUBNET_CONTIGUOUS) == 0)
				{
					ts_count = 1;
                    pUniData->first_timeslot = ts;				
					for ( ; ts <= MAX_TIMESLOTS_NUM; ts++)
						if (HAS_TIMESLOT(pUniData->timeslot_bitmask, ts))
							ts_count++;
	                if (ts_count >= ts_num)
	                {
	                    pUniData->first_timeslot = ts-ts_count;
	                    ts_ok = true;
	                }
					break;
				}
				else
				{
	                ts_count = 1; ts++;
	                for ( ; HAS_TIMESLOT(pUniData->timeslot_bitmask, ts) && ts <= MAX_TIMESLOTS_NUM; ts++)
	                    ts_count++;
	                if (ts_count >= ts_num)
	                {
	                    pUniData->first_timeslot = ts-ts_count;
	                    ts_ok = true;
	                    break;
	                }
				}
            }
        }
        if (!ts_ok)
        {
            LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "Warning (syncTimeslotsMap): insufficient number of contigious time slots for this request.\n");
        }
    }
    return ret;
}

bool SwitchCtrl_Session_SubnetUNI::verifyTimeslotsMap() 
{
    uint8 timeslots[MAX_TIMESLOTS_NUM/8]; //changing nothing in the actual UNIdata
    SubnetUNI_Data* pUniData = isSource ? &subnetUniSrc : &subnetUniDest;
    memcpy(timeslots, pUniData->timeslot_bitmask, MAX_TIMESLOTS_NUM/8);
    bool ret = syncTimeslotsMapVCG_TL1(timeslots);

    //TODO: make this an inline function...
    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
    assert (sonet_tb1);
    uint8 ts_num = 0;
    switch (sonet_tb1->getSignalType())
    {
    case SONET_TSpec::S_STS1SPE_VC3:
    case SONET_TSpec::S_STS1_STM0:
        ts_num = sonet_tb1->getNCC();
        if (ptpCatUnit == CATUNIT_150MBPS)
            ts_num = ((ts_num+2)/3)*3;
        break;

    case SONET_TSpec::S_STS3CSPE_VC4:
    case SONET_TSpec::S_STS3_STM1:
        ts_num = sonet_tb1->getNCC() * 3;
        break;
    }

    if (ret)
    {
        uint8 ts, ts_count = 0;
        bool ts_ok = false;
		if ((pUniData->options & IFSWCAP_SPECIFIC_SUBNET_CONTIGUOUS) == 0)
		{
			for (ts = pUniData->first_timeslot; ts <= MAX_TIMESLOTS_NUM; ts++)
				if (HAS_TIMESLOT(pUniData->timeslot_bitmask, ts))
					ts_count++;
			if (ts_count >= ts_num)
			{
				ts_ok = true;
			}
		}
		else
		{
			for (ts = pUniData->first_timeslot; ts <= MAX_TIMESLOTS_NUM && HAS_TIMESLOT(timeslots, ts); ts++)
	        {
	            ts_count++;
	            if (ts_count >=  ts_num)
	            {
	                ts_ok = true;
	                break;
	            }
	        }
		}
        if (!ts_ok)
        {
            LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "Warning (verifyTimeslotsMap): the range of contigious timeslots suggested by signaling may overlap with existing VCGs.\n");
        } 
    }
    return ret;
}

//return 0 if all snc's have been in stable working state or return id (1-4) of the first group/snc that isn't working
bool SwitchCtrl_Session_SubnetUNI::hasSNCInStableWorkingState()
{
    int interval = 10;
    int countDown = 6;
    while (countDown-- > 0)
    {
        sleep(interval);
        int ret = verifySNCInStableWorkingState_TL1(currentSNC);
        if (ret == 0)
        {
            LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "verifySNCInStableWorkingState confirmed the SNC(s) are in stable working state.\n");
            return true;
        }
        else if (ret < 0)
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", "verifySNCInStableWorkingState found SNC#", -ret, " in error or unstable state.\n");
            return false;
        }
        else //ret > 0 --> neither working or error wait 10 more seconds
        {
            LOG(8)(Log::MPLS, "LSP=", currentLspName, ": ", "verifySNCInStableWorkingState return ", ret, "...  will continue polling after ", interval, " seconds. \n");
        }
    }

    LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "verifySNCInStableWorkingState failed to confirm that all SNCs are in stable working state after 60 seconds.\n");
    return false;
}


bool SwitchCtrl_Session_SubnetUNI::hasSystemSNCHolindgCurrentVCG_TL1(bool& noError)
{
    int ret = 0;
    noError = true;
    SubnetUNI_Data *pUniData = &subnetUniDest;
    if (pUniData->first_timeslot == 0 || pUniData->first_timeslot > MAX_TIMESLOTS_NUM || pUniData->logical_port == 0)
    {
        LOG(4)(Log::MPLS, "LSP=", currentLspName, ": ", "invalid subnetUniDest information.\n");
        return false;
    }

    String OMPortString, ETTPString;
    char* pstr;
    int ts1;
    char fromEndPointPattern2[20];
    getCienaLogicalPortString(OMPortString, ETTPString, ntohl(pUniData->logical_port));
    sprintf(fromEndPointPattern2, "_%s_S", OMPortString.chars());

    sprintf(bufCmd, "rtrv-snc-stspc::all:%d;\r", getNewCtag());
    if ( (ret = writeShell(bufCmd, 5)) < 0 ) goto _out;

    sprintf(strCOMPLD, "M  %d COMPLD", getCurrentCtag());
    sprintf(strDENY, "M  %d DENY", getCurrentCtag());
    ret = readShell(strCOMPLD, strDENY, 1, 5);
    if (ret == 1) 
    {
        ret = readShell("   /* Empty", "   \"", 1, 5);
        if (ret == 1)
        {
            LOG(5)(Log::MPLS, "LSP=", currentLspName, ": ", " hasSystemSNCHolindgCurrentVCG_TL1 method found no SNC holding the current VCG.\n", bufCmd);
            readShell(SWITCH_PROMPT, NULL, 1, 5);
            return false;
        }
        else if (ret == 2)
        {
            while ((ret = ReadShellPattern(bufCmd, (char*)"FROMENDPOINT=dcs_gtp_", fromEndPointPattern2, (char*)"MAXADMINWEIGHT=", (char*)";", 5)) != READ_STOP)
            { // if (ret == 3), we have reach the end, i.e., ";"...
                if (ret == 0) // this is an irrelevant SNC 
                    continue;
                if (ret == 1) // this is an SNC originating at source point...
                    continue;
                else if (ret == 2)
                {
                    pstr = strstr(bufCmd, fromEndPointPattern2);
                    ret = sscanf(pstr+10, "%d", &ts1);
                    if (ret != 1)
                        goto _out;

                    //TODO: make this an inline function...
                    SONET_TSpec* sonet_tb1 = RSVP_Global::switchController->getEosMapEntry(pUniData->ethernet_bw);
                    assert (sonet_tb1);
                    uint8 ts_num = 0;
                    switch (sonet_tb1->getSignalType())
                    {
                    case SONET_TSpec::S_STS1SPE_VC3:
                    case SONET_TSpec::S_STS1_STM0:
                        ts_num = sonet_tb1->getNCC();
                        if (ptpCatUnit == CATUNIT_150MBPS)
                            ts_num = ((ts_num+2)/3)*3;
                        break;

                    case SONET_TSpec::S_STS3CSPE_VC4:
                    case SONET_TSpec::S_STS3_STM1:
                        ts_num = sonet_tb1->getNCC() * 3;
                        break;
                    }
                    //conditions for VCG-owned SNC detection
                    bool detected = false;
                    if ((pUniData->options & IFSWCAP_SPECIFIC_SUBNET_CONTIGUOUS) != 0)
                    {
                        if ((ts1+1 - pUniData->first_timeslot) %48 == 0 && ts1+1 - pUniData->first_timeslot >= 0 && ts1+1 - pUniData->first_timeslot < ts_num)
                            detected = true;
                    }
                    else
                    {
                        int ts_count = 0;
                        for (int ts = pUniData->first_timeslot; ts <= MAX_TIMESLOTS_NUM; ts++)
                        {
                            if (!HAS_TIMESLOT(pUniData->timeslot_bitmask, ts))
                                continue;
                            ts_count++;
                            if (ts_count > ts_num)
                                break;
                            if (ts1+1 == ts)
                            {
                                detected = true;
                                break;				
                            }
                        }
                    }
                    if (detected)
                    {
                        LOG(5)(Log::MPLS, "LSP=", currentLspName, ": ", " hasSystemSNCHolindgCurrentVCG_TL1 method detected an SNC holding the current VCG.\n", bufCmd);
                        ret = readShell(SWITCH_PROMPT, NULL, 1, 5);
                        return true;
                    }
                }
                else
                    goto _out; // wrong
            }
            LOG(5)(Log::MPLS, "LSP=", currentLspName, ": ", " hasSystemSNCHolindgCurrentVCG_TL1 method found no SNC holding the current VCG.\n", bufCmd);
            return false;
        }
        else
            goto _out;
    }
    else if (ret == 2) 
    {
        LOG(5)(Log::MPLS, "LSP=", currentLspName, ": ", " hasSystemSNCHolindgCurrentVCG_TL1 retrieving SNC-STSPC list was denied.\n", bufCmd);
        readShell(SWITCH_PROMPT, NULL, 1, 5);
        return false;
    }
    else
        goto _out;

_out:
    noError = false;
    LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", OMPortString, " hasSystemSNCHolindgCurrentVCG_TL1 method via TL1_TELNET failed...\n", bufCmd);
    return false;
}

bool SwitchCtrl_Session_SubnetUNI::waitUntilSystemSNCDisapear()
{
    bool noError = true;
    int counter = 15;
    do {
        if (!noError)
            return false;
        LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", " Child-Process::waitUntilSystemSNCDisapear ... 2x", counter," seconds left.\n");
        sleep(2); //sleeping two second and try again
        counter--;
        if (counter == 0) //timeout!
        {
            LOG(6)(Log::MPLS, "LSP=", currentLspName, ": ", " ### Child-Process::waitUntilSystemSNCDisapear still sees an SCN holding the VCG after ", counter*2, " seconds --> Child-Process aborted!\n");
            return false;
        }
    } while (hasSystemSNCHolindgCurrentVCG_TL1(noError));
    return true;
}

//// For monitoring service API

bool SwitchCtrl_Session_SubnetUNI::getMonSwitchInfo(MON_Reply_Subobject& monReply) {
    if (switchInetAddr.rawAddress() == 0)
    {
        monReply.switch_options |= MON_SWITCH_OPTION_ERROR;
        return false;
    }

    if (isSource)
    {
        monReply.switch_options |=  MON_SWITCH_OPTION_SUBNET_SRC;
        monReply.switch_info.switch_ip[0].s_addr = switchInetAddr.rawAddress();
    }
    else
    {
        monReply.switch_options |=  MON_SWITCH_OPTION_SUBNET_DEST;
        monReply.switch_info.switch_ip[1].s_addr = switchInetAddr.rawAddress();
    }
     monReply.switch_info.switch_type = CienaSubnet;
    monReply.switch_info.access_type = CLI_TL1_TELNET;
    sscanf(TL1_TELNET_PORT, "%d", &monReply.switch_info.switch_port);
    monReply.switch_options |= MON_SWITCH_OPTION_SUBNET;
 	
    return true;
}

bool SwitchCtrl_Session_SubnetUNI::getMonCircuitInfo(MON_Reply_Subobject& monReply)
{
    _Subnet_Circuit_Info* eosInfo = NULL;
    SubnetUNI_Data* subnetUniData = NULL;
    if (isSource)
    {
        monReply.switch_options |= MON_SWITCH_OPTION_SUBNET_SRC;
        monReply.length += sizeof(struct _Subnet_Circuit_Info);
        if ((monReply.switch_options & MON_SWITCH_OPTION_SUBNET_DEST) != 0)
            monReply.circuit_info.eos_info[1] = monReply.circuit_info.eos_info[0];
        eosInfo = &monReply.circuit_info.eos_info[0];			
        subnetUniData = &subnetUniSrc;
    }
    else 
    {
        monReply.switch_options |= MON_SWITCH_OPTION_SUBNET_DEST;
        monReply.length += sizeof(struct _Subnet_Circuit_Info);
        if ((monReply.switch_options & MON_SWITCH_OPTION_SUBNET_SRC) == 0)
            eosInfo = &monReply.circuit_info.eos_info[0];
        else
        {
            eosInfo = &monReply.circuit_info.eos_info[1];
        }
        subnetUniData = &subnetUniDest;
    }
    eosInfo->subnet_id = subnetUniData->subnet_id;
    eosInfo->first_timeslot = subnetUniData->first_timeslot;
    eosInfo->port = ntohl(subnetUniData->logical_port);
    eosInfo->ethernet_bw = subnetUniData->ethernet_bw;
    if (currentVCG.empty())
        return false;
    strncpy(eosInfo->vcg_name, currentVCG.chars(), MAX_MON_NAME_LEN-20);

    if (isTunnelMode)
    {
        monReply.switch_options |= MON_SWITCH_OPTION_SUBNET_TUNNEL;
        String omport, ettp;
        getCienaLogicalPortString(omport, ettp, ntohl(subnetUniData->logical_port));
        sprintf(eosInfo->eflow_in_name, "%s_%s", ettp.chars(), currentVCG.chars());
        sprintf(eosInfo->eflow_out_name, "%s_%s", currentVCG.chars(), ettp.chars());
    }
    else
    {
        sprintf(eosInfo->eflow_in_name, "dcs_eflow_%s_in", currentVCG.chars());
        sprintf(eosInfo->eflow_out_name, "dcs_eflow_%s_out", currentVCG.chars());
    }

    if (!currentSNC.empty())
    {
        monReply.switch_options |= MON_SWITCH_OPTION_SUBNET_SNC;
        strncpy(eosInfo->snc_crs_name, currentSNC.chars(), MAX_MON_NAME_LEN-5);
        char tail[4]; sprintf(tail, "-%d", numGroups > 0 ? numGroups : 1);
        strcat(eosInfo->dtl_name, tail);
    }
    else if (!currentCRS.empty())
    {
        strncpy(eosInfo->snc_crs_name, currentCRS.chars(), MAX_MON_NAME_LEN-1);
    }
    else if (isSource)
        return false;

    if (DTL.count > 0) 
    {
        monReply.switch_options |= MON_SWITCH_OPTION_SUBNET_DTL;
        strncpy(eosInfo->dtl_name, currentSNC.chars(), MAX_MON_NAME_LEN-5);
	 strcat(eosInfo->dtl_name, "-dtl");
    }
    return true;
}

//// For monitoring service API

// SNC stability checking callback via signal

PSB* psbArrayWaitingForStableSNC[NSIG_SNC_STABLE];

int alloc_snc_stable_psb_slot(PSB* psb)
{
	for (int i = 0; i < NSIG_SNC_STABLE; i++)
	{
		if (psbArrayWaitingForStableSNC[i] == NULL)
		{
			psbArrayWaitingForStableSNC[i] = psb;
			return i;
		}
	}
	return -1;
}
void free_snc_stable_psb_slot(PSB* psb)
{
	for (int i = 0; i < NSIG_SNC_STABLE; i++)
	{
		if (psbArrayWaitingForStableSNC[i] == psb)
		{
			psbArrayWaitingForStableSNC[i] = NULL;
		}
	}

}
void sigfunc_snc_stable(int signo)
{
	assert (signo - SIG_SNC_STABLE_BASE >= 0 && signo - SIG_SNC_STABLE_BASE < NSIG_SNC_STABLE);
	PSB* psb = psbArrayWaitingForStableSNC[signo - SIG_SNC_STABLE_BASE];
	assert (psb != NULL);
	RSVP_Global::messageProcessor->resurrectResvRefresh(&psb->getSession(), psb->getPHopSB());
	psbArrayWaitingForStableSNC[signo - SIG_SNC_STABLE_BASE] = NULL;
	signal(signo, SIG_IGN);
	SignalHandling::userSignal = true;
}


bool SwitchCtrl_Session_SubnetUNI::IsSubnetTransitERO(const EXPLICIT_ROUTE_Object * explicitRoute)
{
    if (!explicitRoute || explicitRoute->getAbstractNodeList().size() < 2)
        return false;
    bool isIPv4Only = (explicitRoute->getAbstractNodeList().front().getType() == AbstractNode::IPv4);
    AbstractNodeList::ConstIterator iter = explicitRoute->getAbstractNodeList().begin();
    for ( ; iter !=  explicitRoute->getAbstractNodeList().end(); ++iter) {
        AbstractNode& node = const_cast<AbstractNode&>(*iter);
        if (node.getType() == AbstractNode::UNumIfID) {
            if(isIPv4Only && ((node.getInterfaceID()>>16) == LOCAL_ID_TYPE_SUBNET_UNI_DEST || (node.getInterfaceID()>>16) == LOCAL_ID_TYPE_SUBNET_IF_ID))
                return true;
	     else
                return false;
        }
        if (node.getType() != AbstractNode::IPv4)
            isIPv4Only = false;
    }
    return false;
}


/****************************************************************************

Cisco (vendor) Catalyst 3750 (model) Control Module header file SwitchCtrl_Session_Catalyst3750.h
Created by Ajay Todimala, 2007
Modified by Xi Yang, 2008
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#ifndef SWITCHCTRL_SESSION_CATALYST3750_H_
#define SWITCHCTRL_SESSION_CATALYST3750_H_

#include "SNMP_Session.h"
#include "CLI_Session.h"

#define CATALYST3750_MIN_VLAN_ID 1
#define CATALYST3750_MAX_VLAN_ID 4094
#define CATALYST3750_MIN_PORT_ID 0
#define CATALYST3750_MAX_PORT_ID 2048
#define CATALYST_VLAN_BITLEN		4096

#ifndef CISCO_ERROR_PROMPT 
#define CISCO_ERROR_PROMPT "% "
#endif

class SwitchCtrl_Session_Catalyst3750_CLI: public CLI_Session
{
public:
	SwitchCtrl_Session_Catalyst3750_CLI(): CLI_Session() { }	
	SwitchCtrl_Session_Catalyst3750_CLI(const String& sName, const NetAddress& swAddr): CLI_Session(sName, swAddr) { }
	virtual ~SwitchCtrl_Session_Catalyst3750_CLI() { }

	virtual bool preAction();
	virtual bool postAction();
	///////////------VLAN Functions ------/////////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID);
        virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
        bool removeUntaggedPortFromVLAN(uint32 port);
	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	//Vendor/Model specific hook functions --> not used (for compile only)
        virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars) { }
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port) { return false;}
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID) { return false;}
	friend class SwitchCtrl_Session_Catalyst3750;
private:
	bool getPortNameById(char* portName, uint32 portID);
};

class SwitchCtrl_Session_Catalyst3750: public SNMP_Session
{
public:
	SwitchCtrl_Session_Catalyst3750(): SNMP_Session() { rfc2674_compatible = false; snmp_enabled = true; activeVlanId = 0; }
	SwitchCtrl_Session_Catalyst3750(const RSVP_String& sName, const NetAddress& swAddr): SNMP_Session(sName, swAddr), cliSession(sName, swAddr) 
		{ rfc2674_compatible = false; snmp_enabled = true; activeVlanId = 0;}
	virtual ~SwitchCtrl_Session_Catalyst3750() { this->disconnectSwitch(); }

	virtual bool refresh() { if (CLI_SESSION_TYPE == CLI_NONE) return true; return cliSession.refresh(); }
	virtual bool connectSwitch();
	virtual void disconnectSwitch();

	///////////------QoS Functions ------/////////
	virtual bool policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);
	virtual bool limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size=0, float peak_rate=0.0,  int peak_burst_size=0);

	////////-----Vendor/Model specific hook functions------//////
	virtual bool hook_createVLAN(const uint32 vlanID);
	virtual bool hook_removeVLAN(const uint32 vlanID);
	virtual bool hook_isVLANEmpty(const vlanPortMap &vpm);
	virtual void hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars);
	virtual void hook_getVlanMapFromSnmpVars(portVlanMap &pvm, netsnmp_variable_list *vars);
	virtual bool hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port);
	virtual bool hook_getPortListbyVLAN(PortList& portList, uint32  vlanID);

	virtual uint32 hook_convertPortInterfaceToID(uint32 id);
	virtual uint32 hook_convertPortIDToInterface(uint32 id);
	virtual bool hook_createPortToIDRefTable(portRefIDList &convList);
	virtual uint32 hook_convertVLANInterfaceToID(uint32 id) { return id; }
	virtual uint32 hook_convertVLANIDToInterface(uint32 id) { return id; }
	virtual bool hook_createVlanInterfaceToIDRefTable(vlanRefIDList &convList);

	//////////// Functions that need implementation since Catalyst 3750 is Does not support RFC2674 ////////////
	virtual bool verifyVLAN(uint32 vlanID); 
	virtual bool setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID);


	///////------Vendor/model specific functions--------///////
	virtual bool movePortToVLANAsTagged(uint32 port, uint32 vlanID);
	virtual bool movePortToVLANAsUntagged(uint32 port, uint32 vlanID);
	virtual bool removePortFromVLAN(uint32 port, uint32 vlanID);
	virtual uint32 getActiveVlanId(uint32 port) { if (activeVlanId > 0) return activeVlanId; return getVLANbyUntaggedPort(port); }
	virtual bool readVlanPortMapListAllBranch(vlanPortMapList &vpmList);

	///////------Vendor/model specific functions for controlling port --------///////
	virtual bool isPortTurnedOn(uint32 port);
	virtual bool TurnOnPort(uint32 port, bool on);
	virtual bool isPortTrunking(uint32 port);
	virtual bool PortTrunkingOn(uint32 port);
	virtual bool PortTrunkingOff(uint32 port);
	virtual bool PortStaticAccessOn(uint32 port);
	virtual bool PortStaticAccessOff(uint32 port);
	virtual bool SwitchPortOnOff(uint32 port, bool on);
	virtual bool isSwitchport(uint32 port);

private:
	uint16 activeVlanId; 

	bool readTrunkPortVlanMap(portVlanMapList &);
	bool readVlanPortMapListALLFromPortVlanMapList(vlanPortMapList &, portVlanMapList &);

	SwitchCtrl_Session_Catalyst3750_CLI cliSession;


	uint32 convertUnifiedPort2Catalyst3750(uint32 port)
	{
	    portRefIDList::Iterator it;
	    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
	    {
	        if ((*it).port_id == port)
	            return (*it).port_bit;
	    }
	    return 0;
	}

	uint32 convertCatalyst37502UnifiedPort(uint32 port)
	{
	    portRefIDList::Iterator it;
	    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
	    {
	        if ((*it).port_bit == port)
	            return (*it).port_id;
	    }
	    return 0;
	}
};


#endif /*SWITCHCTRL_SESSION_CATALYST3750_H_*/

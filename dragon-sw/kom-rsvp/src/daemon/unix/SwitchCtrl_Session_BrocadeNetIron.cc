/****************************************************************************

CLI Based Switch Control Module source file SwitchCtrl_Session_BrocadeNetIron.cc
Created by Xi Yang @ 02/27/2012
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Global.h"
#include "SwitchCtrl_Session_BrocadeNetIron.h"
#include "RSVP.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_BrocadeNetIron::preAction()
{
    if (!active || !pipeAlive())
        return false;
    int n;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( ">", "#", true, 1, 10)) ;
    if (n == 1)
    {
        DIE_IF_NEGATIVE(n= writeShell( "enable\n", 5));
        n= readShell( "Password:", "#", 0, 10);
        if (n == 1) {
            if (strcmp(CLI_ENABPASS, "unknown") != 0) {
                DIE_IF_NEGATIVE(n= writeShell( CLI_ENABPASS, 5)) ;
            } else if (strcmp(CLI_PASSWORD, "unknown") != 0) {
                DIE_IF_NEGATIVE(n= writeShell( CLI_PASSWORD, 5)) ;
            }
            DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( "#", NULL, 1, 10)) ;
        }
    }
    DIE_IF_NEGATIVE(writeShell("configure terminal\n", 5));
    DIE_IF_NEGATIVE(readShell("#", NULL, 1, 10));
    return true;
}

bool SwitchCtrl_Session_BrocadeNetIron::postAction()
{
    if (fdout < 0 || fdin < 0)
        return false;
    DIE_IF_NEGATIVE(writeShell("end\n", 5));
    readShell("#", NULL, 1, 10);
    return true;
}

bool SwitchCtrl_Session_BrocadeNetIron::movePortToVLANAsTagged(uint32 portID, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL;

    if ((!active) || portID==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
        return ret; //don't touch the control port!

    bit = Port2Bit(portID);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
       SetPortBit(vpmAll->portbits, bit);
       ret &= this->addVLANPort_ShellScript(portID, vlanID, true);
    }
    
    return ret;
}


bool SwitchCtrl_Session_BrocadeNetIron::movePortToVLANAsUntagged(uint32 portID, uint32 vlanID)
{
    uint32 bit;
    bool ret = false;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || portID==SWITCH_CTRL_PORT || vlanID<MIN_VLAN || vlanID>MAX_VLAN) 
        return ret; //don't touch the control port!

    int old_vlan = getVLANbyUntaggedPort(portID);
    bit = Port2Bit(portID);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
    if (vpmUntagged)
        ResetPortBit(vpmUntagged->portbits, bit);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
    if (vpmAll)
        ResetPortBit(vpmAll->portbits, bit);
    if (old_vlan > 1) { //Remove untagged port from old VLAN
        ret &= deleteVLANPort_ShellScript(portID, old_vlan, false);
    }

    bit = Port2Bit(portID);
    assert(bit < MAX_VLAN_PORT_BYTES*8);
    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged)
    SetPortBit(vpmUntagged->portbits, bit);
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetPortBit(vpmAll->portbits, bit);
        ret &= this->addVLANPort_ShellScript(portID, vlanID, false);
    }

    return ret;
}


bool SwitchCtrl_Session_BrocadeNetIron::removePortFromVLAN(uint32 portID, uint32 vlanID)
{
    bool ret = false;
    uint32 bit;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;

    if ((!active) || portID==SWITCH_CTRL_PORT)
    	return ret; //don't touch the control port!

    if (vlanID>=MIN_VLAN && vlanID<=MAX_VLAN){
        bit = Port2Bit(portID);
        assert(bit < MAX_VLAN_PORT_BYTES*8);
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
        if (vpmUntagged)
            ResetPortBit(vpmUntagged->portbits, bit);
        vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
        if (vpmAll) {
            ResetPortBit(vpmAll->portbits, bit);
            bool ret2 = this->deleteVLANPort_ShellScript(portID, vlanID, true);
            bool ret1 = this->deleteVLANPort_ShellScript(portID, vlanID, false);
            ret &= (ret1 || ret2);
        }
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }

    return ret;
}


bool SwitchCtrl_Session_BrocadeNetIron::addVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool tagged)
{
    int n;
    uint32 port_part,slot_part;
    char portName[16], vlanNum[16];
    
    port_part=(portID)&0xff;
    slot_part=(portID>>8)&0xf;
    
    if (!preAction())
        return false;
    
    sprintf(portName, "%d/%d", slot_part, port_part);
    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);    
    if (tagged)
        DIE_IF_NEGATIVE(n = writeShell( "tagged ethernet ", 5));
    else
        DIE_IF_NEGATIVE(n = writeShell( "untagged ethernet ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);

    return postAction();
}

bool SwitchCtrl_Session_BrocadeNetIron::deleteVLANPort_ShellScript(uint32 portID, uint32 vlanID, bool tagged)
{

    int n;
    uint32 port_part,slot_part;
    char portName[16], vlanNum[16];

    port_part=(portID)&0xff;
    slot_part=(portID>>8)&0xf;

    if (!preAction())
        return false;

    sprintf(portName, "%d/%d", slot_part, port_part);
    sprintf(vlanNum, "%d", vlanID);

    DIE_IF_NEGATIVE(n = writeShell( "vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);
    if (tagged)
        DIE_IF_NEGATIVE(n = writeShell( "no tagged ethernet ", 5));
    else
        DIE_IF_NEGATIVE(n = writeShell( "no untagged ethernet ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", "error", true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);

    return postAction();
}

bool SwitchCtrl_Session_BrocadeNetIron::hook_createVLAN(const uint32 vlanID)
{
    int n;
    char vlanNum[16];

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( " name vlan", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;    
    if (n == 2) readShell(  "#", NULL, 1, 10);
    DIE_IF_NEGATIVE(n = writeShell( "no spanning-tree\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);
    DIE_IF_NEGATIVE(n = writeShell( "no rstp\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);

    return postAction();
}
bool SwitchCtrl_Session_BrocadeNetIron::hook_removeVLAN(const uint32 vlanID)
{
    int n;
    char vlanNum[16];

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "no vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);
    return postAction();
}

bool SwitchCtrl_Session_BrocadeNetIron::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    int n;
    char vlanNum[16];
    char buf[1024];

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vpm.vid);
    DIE_IF_NEGATIVE(n = writeShell( "show vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    n= ReadShellPattern(buf, (char*)"No ports associated",  (char*)"not configured", (char*)"#", (char*)BROCADE_ERROR_PROMPT, 5);
    if (n == 1 || n == 2)
    {
        postAction();
        return true;
    }
    if (n == READ_STOP) readShell( "#", NULL, 1, 10);
    postAction();
    return false;
}

void SwitchCtrl_Session_BrocadeNetIron::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    memset(&vpm, 0, sizeof(vlanPortMap));
    if (vars->val.bitstring ){
        for (unsigned int i = 0; i < vars->val_len && i < MAX_VLAN_PORT_BYTES; i++) {
            vpm.portbits[i] = vars->val.bitstring[i];
       }
    }
    vpm.vid = (uint32)vars->name[vars->name_length - 1];
}

bool SwitchCtrl_Session_BrocadeNetIron::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    uint32 port_bit = Port2Bit(port);
    if (port_bit == 0)
        return false;
    return HasPortBit(vpm.portbits, port_bit);
}

bool SwitchCtrl_Session_BrocadeNetIron::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
{
    uint32 port;
    uint32 bit;
    vlanPortMap* vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if(!vpmAll)
        return false;

    portList.clear();
    for (bit = 0; bit < sizeof(vpmAll->portbits)*8; bit++)
    {
        if (HasPortBit(vpmAll->portbits, bit))
        {
            port = bit+1;
            port = Bit2Port(port);
            if (port != 0)
                portList.push_back(port);
        }
    }

    if (portList.size() == 0)
        return false;
    return true;
}


//committed_rate in bit/second, burst_size in bits
bool SwitchCtrl_Session_BrocadeNetIron::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if (RSVP_Global::switchController->hasSwitchVlanOption(SW_VLAN_NO_QOS)) 
       return true;

    int n;
    uint32 port_part,slot_part;
    char portName[100], vlanNum[100], action[100];
    int committed_rate_int = (int)committed_rate;

    if (committed_rate_int < 1 || !preAction())
        return false;

    port_part=(input_port)&0xff;
    slot_part=(input_port>>8)&0xf;

    sprintf(portName, "%d/%d", slot_part, port_part);
    sprintf(vlanNum, "%d", vlan_id);


    if (burst_size == 0) burst_size = 10000000;
    
    sprintf(action, "%srate-limit input vlan-id %d %d %d", do_undo? "": "no ", vlan_id, committed_rate_int, burst_size);

    // enter interface/port configuration mode 
    DIE_IF_NEGATIVE(n= writeShell( "interface ", 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( portName, 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    DIE_IF_NEGATIVE(n= writeShell( action, 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    if (!postAction())
        return false;
    return true;
}

bool SwitchCtrl_Session_BrocadeNetIron::limitOutputBandwidth(bool do_undo, uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size,  float peak_rate, int peak_burst_size)
{
    if (RSVP_Global::switchController->hasSwitchVlanOption(SW_VLAN_NO_QOS)) 
       return true;

    int n;
    uint32 port_part,slot_part;
    char portName[100], vlanNum[100], action[100];
    char append[20];
    int committed_rate_int = (int)committed_rate;

    if (committed_rate_int < 1 || !preAction())
        return false;

    port_part=(output_port)&0xff;
    slot_part=(output_port>>8)&0xf;

    sprintf(portName, "%d/%d", slot_part, port_part);
    sprintf(vlanNum, "%d", vlan_id);


    if (burst_size == 0) burst_size = 10000000;
    
    sprintf(action, "%srate-limit output vlan-id %d %d %d", do_undo? "": "no ", vlan_id, committed_rate_int, burst_size);

    // enter interface/port configuration mode 
    DIE_IF_NEGATIVE(n= writeShell( "interface ", 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( portName, 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    DIE_IF_NEGATIVE(n= writeShell( action, 5)) ;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( "#", BROCADE_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell(  "#", NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    if (!postAction())
        return false;
    return true;
}
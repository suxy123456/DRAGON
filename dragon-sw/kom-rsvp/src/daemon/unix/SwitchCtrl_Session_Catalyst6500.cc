/****************************************************************************

Cisco (vendor) Catalyst 6500 (model) Control Module source file SwitchCtrl_Session_Catalyst6500.cc
Created by Ajay Todimala, 2007
Modified by Xi Yang, 2008
QoS feature added by Xi Yang, 09/2008
To be incorporated into KOM-RSVP-TE package

****************************************************************************/

#include "SwitchCtrl_Session_Catalyst6500.h"
#include "RSVP_Log.h"

bool SwitchCtrl_Session_Catalyst6500_CLI::preAction()
{
    if (!active || vendor!=Catalyst6500|| !pipeAlive())
        return false;
    int n;
    DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( ">", "#", true, 1, 10)) ;
    if (n == 1)
    {
        DIE_IF_NEGATIVE(n= writeShell( "enable\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "Password: ", NULL, 0, 10)) ;
        if (strcmp(CLI_ENABPASS, "unknown") != 0) {
            DIE_IF_NEGATIVE(n= writeShell( CLI_ENABPASS, 5)) ;
        } else if (strcmp(CLI_PASSWORD, "unknown") != 0) {
            DIE_IF_NEGATIVE(n= writeShell( CLI_PASSWORD, 5)) ;
        }
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    }   
    DIE_IF_NEGATIVE(n= writeShell( "configure\n\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    DIE_IF_NEGATIVE(n= writeShell( "mls qos\n", 5)) ;
    DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    return true;
}

bool SwitchCtrl_Session_Catalyst6500_CLI::postAction()
{
    int n;
    if (fdout < 0 || fdin < 0)
        return false;
    DIE_IF_NEGATIVE(writeShell("end\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    return true;
}

bool SwitchCtrl_Session_Catalyst6500_CLI::movePortToVLANAsTagged(uint32 portID, uint32 vlanID)
{
    int n;
    uint32 port_part,slot_part;
    char portName[16], vlanNum[16];

    port_part=(portID)&0xff;     
    slot_part=(portID>>8)&0xf;
    switch(RSVP_Global::switchController->getSlotType(slot_part, port_part)) {
    case SLOT_TYPE_GIGE:
        sprintf(portName, "gi%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_TENGIGE:
        sprintf(portName, "te%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_ILLEGAL:
    default:
        return false;
    }

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);
    
    DIE_IF_NEGATIVE(n = writeShell( "switchport trunk allowed vlan add ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    return postAction();
}


bool SwitchCtrl_Session_Catalyst6500_CLI::movePortToVLANAsUntagged(uint32 portID, uint32 vlanID)
{
    int n;
    uint32 port_part,slot_part;
    char portName[16], vlanNum[16];
    
    port_part=(portID)&0xff;     
    slot_part=(portID>>8)&0xf;
    switch(RSVP_Global::switchController->getSlotType(slot_part, port_part)) {
    case SLOT_TYPE_GIGE:
        sprintf(portName, "gi%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_TENGIGE:
        sprintf(portName, "te%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_ILLEGAL:
    default:
        return false;
    }

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);
    
    DIE_IF_NEGATIVE(n = writeShell( "switchport access vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    return postAction();

}

// for trunk/tagged port only
bool SwitchCtrl_Session_Catalyst6500_CLI::removePortFromVLAN(uint32 portID, uint32 vlanID)
{
    int n;
    uint32 port_part,slot_part;
    char portName[16], vlanNum[16];
    
    port_part=(portID)&0xff;     
    slot_part=(portID>>8)&0xf;
    switch(RSVP_Global::switchController->getSlotType(slot_part, port_part)) {
    case SLOT_TYPE_GIGE:
        sprintf(portName, "gi%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_TENGIGE:
        sprintf(portName, "te%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_ILLEGAL:
    default:
        return false;
    }

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);
    
    DIE_IF_NEGATIVE(n = writeShell( "switchport trunk allowed vlan remove ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    return postAction();
}

bool SwitchCtrl_Session_Catalyst6500_CLI::removeUntaggedPortFromVLAN(uint32 portID)
{
    int n;
    uint32 port_part,slot_part;
    char portName[16];
    
    port_part=(portID)&0xff;     
    slot_part=(portID>>8)&0xf;
    switch(RSVP_Global::switchController->getSlotType(slot_part, port_part)) {
    case SLOT_TYPE_GIGE:
        sprintf(portName, "gi%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_TENGIGE:
        sprintf(portName, "te%d/%d",slot_part,port_part);
        break;
    case SLOT_TYPE_ILLEGAL:
    default:
        return false;
    }

    if (!preAction())
        return false;

    
    DIE_IF_NEGATIVE(n = writeShell( "interface ", 5));
    DIE_IF_NEGATIVE(n = writeShell( portName, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);
    
    DIE_IF_NEGATIVE(n = writeShell( "switchport access vlan 1", 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    DIE_IF_NEGATIVE(n = writeShell( "no switchport", 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    return postAction();
}

bool SwitchCtrl_Session_Catalyst6500_CLI::hook_createVLAN(const uint32 vlanID)
{
    int n;
    char vlanNum[16];

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    return postAction();
}
bool SwitchCtrl_Session_Catalyst6500_CLI::hook_removeVLAN(const uint32 vlanID)
{
    int n;
    char vlanNum[16];

    if (!preAction())
        return false;

    sprintf(vlanNum, "%d", vlanID);
    
    DIE_IF_NEGATIVE(n = writeShell( "no vlan ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
    DIE_IF_EQUAL(n, 2);

    return postAction();
}

bool SwitchCtrl_Session_Catalyst6500_CLI::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    int n;
    char vlanNum[16];
    char buf[1024];

    if (!preAction())
        return false;

    DIE_IF_NEGATIVE(writeShell("end\n", 5));
    DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
    if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);

    sprintf(vlanNum, "%d", vpm.vid);
    DIE_IF_NEGATIVE(n = writeShell( "show vlan id ", 5));
    DIE_IF_NEGATIVE(n = writeShell( vlanNum, 5));
    DIE_IF_NEGATIVE(n = writeShell( "\n", 5));
    n= ReadShellPattern(buf, (char*)"active    Gi",  (char*)"active    Te", (char*)"#", (char*)CISCO_ERROR_PROMPT, 5);
    if (n == 0)
    {
        postAction();
        return true;
    }
    if (n == READ_STOP) readShell( SWITCH_PROMPT, NULL, 1, 10);
    postAction();
    return false; //matching pattern 1 or 2 or there is error
}

//committed_rate in bit/second, burst_size in bytes
bool SwitchCtrl_Session_Catalyst6500_CLI::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    int n;
    uint32 port,slot, shelf;
    char portName[50], vlanNum[10], action[100], policyMapName[100];
    long long committed_rate_int = (long long)committed_rate;

    if (committed_rate_int < 1 || !preAction())
        return false;

    port=(input_port)&0xff;
    slot=(input_port>>8)&0xf;
    shelf = (input_port>>12)&0xf;

    switch(RSVP_Global::switchController->getSlotType(slot, port)) {
    case SLOT_TYPE_GIGE:
        if (shelf == 0)
            sprintf(portName, "gi%d/%d", slot, port);
        else
            sprintf(portName, "gi%d/%d/%d",shelf, slot, port);
        break;
    case SLOT_TYPE_TENGIGE:
        if (shelf == 0)
            sprintf(portName, "te%d/%d", slot, port);
        else
            sprintf(portName, "te%d/%d/%d",shelf, slot, port);
        break;
    case SLOT_TYPE_ILLEGAL:
    default:
        return false;
    }

    // create or delete policy
    sprintf(vlanNum, "%d", vlan_id);
    sprintf(policyMapName, "policy-map-vlan%d", vlan_id);
    if (do_undo)
    {
        committed_rate_int *= 1000000;
        if (burst_size < 500) 
            burst_size = 500000;
        else
            burst_size *= 1000;
        if (peak_burst_size <= 500) 
            peak_burst_size = burst_size;
        sprintf(action, "police %lld %d %d conform-action transmit exceed-action drop violate-action drop", 
    		committed_rate_int, burst_size, peak_burst_size); //excess burst size = peak_burst_size -burst_size
        DIE_IF_NEGATIVE(n= writeShell( "policy-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( policyMapName, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( "class class-default\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( action, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        // return to configure root
        DIE_IF_NEGATIVE(n= writeShell( "end\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "configure\n\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;

        // enter interface port configuration mode 
        DIE_IF_NEGATIVE(n= writeShell( "interface ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( portName, 5)) ; // try port as GigE interface
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10));
        if (n ==2) // try port again as TenGigE interface
        {
            readShell( SWITCH_PROMPT, NULL, 1, 10);
            if (shelf == 0)
                sprintf(portName, "te%d/%d", slot, port);
            else
                sprintf(portName, "te%d/%d/%d",shelf, slot, port);
            DIE_IF_NEGATIVE(n= writeShell( "interface ", 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( portName, 5)) ; // try GigE interface
            DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
            if (n == 2) 
            {
                readShell( SWITCH_PROMPT, NULL, 1, 10);
                LOG(1)(Log::MPLS, "Error: SwitchCtrl_Session_Catalyst6500_CLI::policeInputBandwidth failed (interface 'port')");
            }
            DIE_IF_EQUAL(n, 2);
        }
        // set mls qos to 'vlan based' for the port
        DIE_IF_NEGATIVE(n= writeShell( "mls qos vlan-based\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) 
        {
            readShell( SWITCH_PROMPT, NULL, 1, 10);
            LOG(1)(Log::MPLS, "Error: SwitchCtrl_Session_Catalyst6500_CLI::policeInputBandwidth failed (mls qos vlan-based)");
        }
        DIE_IF_EQUAL(n, 2);
        // set mtu to 9216 for the port
        DIE_IF_NEGATIVE(n= writeShell( "mtu 9216\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) 
        {
            readShell( SWITCH_PROMPT, NULL, 1, 10);
            LOG(1)(Log::MPLS, "Error: SwitchCtrl_Session_Catalyst6500_CLI::policeInputBandwidth failed (mtu 9216')");
        }
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( "exit\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
    
        // enter interface vlan configuration mode 
        DIE_IF_NEGATIVE(n= writeShell( "interface vlan ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( vlanNum, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) 
        {
            readShell( SWITCH_PROMPT, NULL, 1, 10);
            LOG(1)(Log::MPLS, "Error: SwitchCtrl_Session_Catalyst6500_CLI::policeInputBandwidth failed (interface vlan)");
        }
        DIE_IF_EQUAL(n, 2);
        // set mtu to 9216 for the vlan interface
        DIE_IF_NEGATIVE(n= writeShell( "mtu 9216\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
        DIE_IF_EQUAL(n, 2);
        // apply policy
        DIE_IF_NEGATIVE(n= writeShell( "no shutdown\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( SWITCH_PROMPT, NULL, 1, 10)) ;
        DIE_IF_NEGATIVE(n= writeShell( "service-policy input ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( policyMapName, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) 
        {
            readShell( SWITCH_PROMPT, NULL, 1, 10);
            LOG(1)(Log::MPLS, "Error: SwitchCtrl_Session_Catalyst6500_CLI::policeInputBandwidth failed (interface vlan --> service-policy input)");
        }
        DIE_IF_EQUAL(n, 2);
        DIE_IF_NEGATIVE(n= writeShell( "service-policy output ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( policyMapName, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) 
        {
            readShell( SWITCH_PROMPT, NULL, 1, 10);
            LOG(1)(Log::MPLS, "Error: SwitchCtrl_Session_Catalyst6500_CLI::policeInputBandwidth failed (interface vlan --> service-policy input)");
        }
        DIE_IF_EQUAL(n, 2);

    }
    else
    {
        // remove policy-map
        DIE_IF_NEGATIVE(n= writeShell( "no policy-map ", 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( policyMapName, 5)) ;
        DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
        DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
        if (n == 2) 
        {
            readShell( SWITCH_PROMPT, NULL, 1, 10);
            LOG(1)(Log::MPLS, "Error: SwitchCtrl_Session_Catalyst6500_CLI::policeInputBandwidth failed (no policy-map)");
        }
        DIE_IF_EQUAL(n, 2);
        // remove interface vlan
        /*
            DIE_IF_NEGATIVE(n= writeShell( "no interface vlan ", 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( vlanNum, 5)) ;
            DIE_IF_NEGATIVE(n= writeShell( "\n", 5)) ;
            DIE_IF_NEGATIVE(n= readShell( "#", CISCO_ERROR_PROMPT, true, 1, 10)) ;
            if (n == 2) readShell( SWITCH_PROMPT, NULL, 1, 10);
            DIE_IF_EQUAL(n, 2);
            */
    }

    // end
    if (!postAction())
        return false;
    return true;
}

bool SwitchCtrl_Session_Catalyst6500_CLI::limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if (!postAction())
        return false;
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::connectSwitch()
{
    if (SwitchCtrl_Session::connectSwitch() == false)
        return false;

    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        cliSession.vendor = this->vendor;
        cliSession.active = true;
        LOG(2)( Log::MPLS, "VLSR: CLI connecting to Catalyst6500 Switch: ", switchInetAddr);
        return cliSession.engage("Username:");
    }

    return true;
}

void SwitchCtrl_Session_Catalyst6500::disconnectSwitch()
{
    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        LOG(2)( Log::MPLS, "VLSR: CLI disconnecting from Catalyst6500 Switch: ", switchInetAddr);
        cliSession.disengage();
        cliSession.active = false;
    }
}

bool SwitchCtrl_Session_Catalyst6500::policeInputBandwidth(bool do_undo, uint32 input_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if (RSVP_Global::switchController->hasSwitchVlanOption(SW_VLAN_NO_QOS)) 
        return true;
    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        return cliSession.policeInputBandwidth(do_undo, input_port, vlan_id, committed_rate, burst_size, peak_rate, peak_burst_size); 
    }
    return false;
}

bool SwitchCtrl_Session_Catalyst6500::limitOutputBandwidth(bool do_undo,  uint32 output_port, uint32 vlan_id, float committed_rate, int burst_size, float peak_rate,  int peak_burst_size)
{
    if (RSVP_Global::switchController->hasSwitchVlanOption(SW_VLAN_NO_QOS)) 
        return true;
    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        return cliSession.limitOutputBandwidth(do_undo, output_port, vlan_id, committed_rate, burst_size, peak_rate, peak_burst_size);
    }
    return false;
}

bool SwitchCtrl_Session_Catalyst6500::PortTrunkingOn(uint32 port)
{

    int i;
    uint32 port_id;
    char oct[3];
    char type, value[500], oid_str[128];

    if (!active) //not initialized or session has been disconnected
        return false;

    if (!isSwitchport(port))
        SwitchPortOnOff(port, true);

    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.3";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "4");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the Port encapsulation (vlanTrunkPortEncapsulationType) to dot1Q failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.13";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "5");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortDynamicState to onNoNegotiate failed.");
        return false;
    }


    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.17";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    value[0]=0;
    strcat(value,"");
    type='x'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortVlansEnabled2k to None failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.18";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    type='x'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortVlansEnabled3k to None failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.19";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    type='x'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortVlansEnabled4k to None failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.4";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    type='x'; 
   
    value[0]=0;
    for (i = 0; i < 128; i++) {
	snprintf(oct, 3, "%.2x", 0);
	strcat(value,oct);
    }

    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortVlansEnabled to None failed.");
        return false;
    }

    return true;
}


bool SwitchCtrl_Session_Catalyst6500::PortTrunkingOff(uint32 port)
{
    char type, value[128], oid_str[128];
    uint32 port_id;

    if (!active) //not initialized or session has been disconnected
        return false;

    port_id = hook_convertPortIDToInterface(port);

    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.13";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "2");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortDynamicState to Off failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.3";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "5");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the Port encapsulation (vlanTrunkPortEncapsulationType) to negotiate failed.");
        return false;
    }

    // Shutdown the port
    SwitchPortOnOff(port, false);

    return true;
}

bool SwitchCtrl_Session_Catalyst6500::PortStaticAccessOn(uint32 port)
{
    char type, value[128], oid_str[128];
    uint32 port_id;

    if (!active) //not initialized or session has been disconnected
        return false;

    if (!isSwitchport(port))
        SwitchPortOnOff(port, true);

    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.3";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "5");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the Port encapsulation (vlanTrunkPortEncapsulationType) to negotiate failed.");
        return false;
    }

    tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.13";
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    strcpy(value, "2");
    type='i'; 
    if (!SNMPSet(oid_str, type, value)) 
    {
        LOG(1)( Log::MPLS, "VLSR: SNMP: Setting the vlanTrunkPortDynamicState to Off failed.");
        return false;
    }

    //add the new *empty* vlan into PortMapListAll and portMapListUntagged
    /* vlanPortMap vpm;
    memset(&vpm, 0, sizeof(vlanPortMap));
    vpm.vid = vlanID;
    vlanPortMapListAll.push_back(vpm);
    memset(vpm.portbits, 0, MAX_VLAN_PORT_BYTES);
    vlanPortMapListUntagged.push_back(vpm); */

    return true;
}

bool SwitchCtrl_Session_Catalyst6500::PortStaticAccessOff(uint32 port)
{
   return SwitchPortOnOff(port, false);
}

bool SwitchCtrl_Session_Catalyst6500::TurnOnPort(uint32 port, bool on)
{
    char type, value[128], oid_str[128];
    if (!active)  return false; 
    uint32 port_id;
	
    String tag_oid_str = ".1.3.6.1.2.1.2.2.1.7";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    if (on) strcpy(value, "1"); 
    else strcpy(value, "2");
    type='i';
    if (!SNMPSet(oid_str, type, value)) 
    {
       LOG(3)( Log::MPLS, "VLSR: SNMP: Turning port ", port, " On/Off failed.");
       return false;
    }
   
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::SwitchPortOnOff(uint32 port, bool on)
{
    char type, value[128], oid_str[128];
    if (!active)  return false; 
    uint32 port_id;
	
    String tag_oid_str = ".1.3.6.1.4.1.9.9.151.1.1.1.1.1";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    if (on) strcpy(value, "2"); 
    else strcpy(value, "1");
    type='i';
    if (!SNMPSet(oid_str, type, value)) 
    {
       LOG(3)( Log::MPLS, "VLSR: SNMP: Turning On/Off switchport on port ", port, " failed.");
       return false;
    }
   
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::isSwitchport(uint32 port)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    int status;
    uint32 port_id;

    if (!active)  
    	return false; 
	

    String tag_oid_str = ".1.3.6.1.4.1.9.9.151.1.1.1.1.1";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    status = read_objid(oid_str, anOID, &anOID_len);

    // Create the PDU for the data for our request.
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out.
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
    {
        if (response->variables->type == 128 || response->variables->type == 129) { //NoSuchObject or NoSuchInstance
            snmp_free_pdu(response);
            return false;
        }     
       vars = response->variables;
       bool ret = ((*(vars->val.integer)) ==2);
    	snmp_free_pdu(response);
	return ret;
    }
    else {
       if (status == STAT_SUCCESS){
          LOG(4)( Log::MPLS, "VLSR: SNMP: Reading switchport ", port, " information failed. Reason : ", snmp_errstring(response->errstat));
       }
       else {
      	   snmp_sess_perror("snmpset", snmpSessionHandle);
          LOG(3)( Log::MPLS, "VLSR: SNMP: Reading switchport ", port, " information failed with STAT_ERROR returned");
       }
       if(response) snmp_free_pdu(response);
       return false;
    }
   
    return true;
}


bool SwitchCtrl_Session_Catalyst6500::isPortTurnedOn(uint32 port)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    int status;
    uint32 port_id;

    if (!active)  
    	return false; 
	
    String tag_oid_str = ".1.3.6.1.2.1.2.2.1.7";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    status = read_objid(oid_str, anOID, &anOID_len);

    // Create the PDU for the data for our request.
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out.
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
    {
        if (response->variables->type == 128 || response->variables->type == 129) { //NoSuchObject or NoSuchInstance
            snmp_free_pdu(response);
            return false;
        }     
       vars = response->variables;
    	snmp_free_pdu(response);
	if ((*(vars->val.integer)) ==1) return true;
	else if ((*(vars->val.integer)) ==1) return false;
    }
    else {
       if (status == STAT_SUCCESS){
          LOG(4)( Log::MPLS, "VLSR: SNMP: Reading port ", port, " information failed. Reason : ", snmp_errstring(response->errstat));
       }
       else {
      	   snmp_sess_perror("snmpget", snmpSessionHandle);
          LOG(3)( Log::MPLS, "VLSR: SNMP: Reading switchport ", port, " information failed with STAT_ERROR returned");
       }
       if(response) snmp_free_pdu(response);
       return false;
    }
   
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::movePortToVLANAsUntagged(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;
    char type, value[300], oid_str[128];
    uint32 port_id, port_bit;

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<CATALYST6500_MIN_VLAN_ID || vlanID>CATALYST6500_MAX_VLAN_ID) 
    	return false; //don't touch the control port!

    if (isPortTrunking(port))
        PortTrunkingOff(port);
    
    PortStaticAccessOn(port);

    port_id = hook_convertPortIDToInterface(port);
    port_bit = convertUnifiedPort2Catalyst6500(port);
    if (port_bit == 0)
    {
        LOG(2)( Log::MPLS, "VLSR: SNMP: Unknown port ", port);
        return false;
    }
    
    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {    
        if (!cliSession.movePortToVLANAsUntagged(port, vlanID))
            return false;
    }
    else // SNMP
    {
        String tag_oid_str = ".1.3.6.1.4.1.9.9.68.1.2.2.1.2";
        sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
        type='i'; 
        sprintf(value, "%d", vlanID);
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(2)( Log::MPLS, "VLSR: SwitchCtrl_Session_Catalyst6500::movePortToVLANAsUntagged: Unknown port ", port);
            return false;
        }
    }
    int old_vlan = getVLANbyUntaggedPort(port);
    if (old_vlan) { //Remove untagged port from old VLAN
        //uint32 mask=(~(1<<(32-port))) & 0xFFFFFFFF;
        vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, old_vlan);
        if (vpmUntagged)
            //vpmUntagged->ports&=mask;
            ResetBit(vpmUntagged->portbits, port_bit-1);
        vpmAll = getVlanPortMapById(vlanPortMapListAll, old_vlan);
        if (vpmAll)
    	    //vpmAll->ports&=mask;
            ResetBit(vpmAll->portbits, port_bit-1);

        //Set original ports back to their "tagged" or "untagged" states
        if (vpmUntagged) setVlanPortMapById(vlanPortMapListUntagged, old_vlan, &vpmUntagged->portbits[0]); 

        //remove THIS untagged port out of the old VLAN
        if (vpmAll) setVlanPortMapById(vlanPortMapListAll, old_vlan, &vpmAll->portbits[0]); 
    }

    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged) { //bit==1 means port is untagged
        SetPortBit(vpmUntagged->portbits, port_bit-1);
        setVlanPortMapById(vlanPortMapListUntagged, vlanID, &vpmUntagged->portbits[0]); 
    }
    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
       SetPortBit(vpmAll->portbits, port_bit-1);
        setVlanPortMapById(vlanPortMapListAll, vlanID, &vpmAll->portbits[0]); 
    }
    else {
       LOG(3)( Log::MPLS, "VLSR: SwitchCtrl_Session_Catalyst6500::movePortToVLANAsUntagged --> getVlanPortMapById (vlanPortMapListAll, vlan=", vlanID, ") failed.");
       return false;
    }

    activeVlanId = vlanID; //$$
    return ret;
}

bool SwitchCtrl_Session_Catalyst6500::movePortToVLANAsTagged(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[500], oid_str[128], oct[3];
    int status, i;
    portVlanMap vlanmap;
    uint32 port_id, port_bit;
    String tag_oid_str[4] = { ".1.3.6.1.4.1.9.9.46.1.6.1.1.4", ".1.3.6.1.4.1.9.9.46.1.6.1.1.17", \
    				".1.3.6.1.4.1.9.9.46.1.6.1.1.18", ".1.3.6.1.4.1.9.9.46.1.6.1.1.19"};

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<CATALYST6500_MIN_VLAN_ID || vlanID>CATALYST6500_MAX_VLAN_ID) 
    	return false; //don't touch the control port!
	
    if (isSwitchport(port)) {
       if (!isPortTrunking(port)) {
          PortStaticAccessOff(port); 
          PortTrunkingOn(port);
       }
    }
    else 
        PortTrunkingOn(port);

    // Get the current vlan mapping for the port
    port_id = hook_convertPortIDToInterface(port);
    port_bit = convertUnifiedPort2Catalyst6500(port);
    if (port_bit == 0)
    {
        LOG(2)( Log::MPLS, "VLSR: SwitchCtrl_Session_Catalyst6500::movePortToVLANAsTagged: Unknown port ", port);
        return false;
    }

    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        if (!cliSession.movePortToVLANAsTagged(port, vlanID))
            return false;
    }
    else // SNMP
    {
        sprintf(oid_str, "%s.%d", tag_oid_str[(vlanID-1)/1024].chars(), port_id);
        status = read_objid(oid_str, anOID, &anOID_len);

        // Create the PDU for the data for our request.
        pdu = snmp_pdu_create(SNMP_MSG_GET);
        snmp_add_null_var(pdu, anOID, anOID_len);
        // Send the Request out.
        status = snmp_synch_response(snmpSessionHandle, pdu, &response);
        if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
        {
           vars = response->variables;
    	hook_getVlanMapFromSnmpVars(vlanmap, vars);
        	snmp_free_pdu(response);
        }
        else {
           if (status == STAT_SUCCESS){
              LOG(4)( Log::MPLS, "VLSR: SNMP: Reading Vlan map of Trunk port ", port, "failed. Reason : ", snmp_errstring(response->errstat));
           }
           else {
          	   snmp_sess_perror("snmpset", snmpSessionHandle);
              LOG(3)( Log::MPLS, "VLSR: SNMP: Reading Vlan map of Trunk port ", port, " failed with STAT_ERROR returned");
           }
           if(response) snmp_free_pdu(response);
           return false;
        }
       
        uint8 mask=((1<<(7-(vlanID%8)))) & 0xFF;
        vlanmap.vlanbits[vlanID/8 - ((uint32)(vlanID-1)/1024)*128] |= mask;

        // Set the vlan mapping for the port
        sprintf(oid_str, "%s.%d", tag_oid_str[(vlanID-1)/1024].chars(), port_id);
        value[0] = 0;
        for (i = 0; i < 128; i++) {
            snprintf(oct, 3, "%.2x", vlanmap.vlanbits[i]);
    	strcat(value,oct);
        }
        type='x';

        if (!SNMPSet(oid_str, type, value)) 
        {
           LOG(3)( Log::MPLS, "VLSR: SNMP: Setting Vlan map of Trunk port ", port, "failed.");
           return false;
        }
    }

    vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
    if (vpmAll) {
        SetBit(vpmAll->portbits, port_bit-1);
    } else {
        LOG(3)( Log::MPLS, "VLSR: SwitchCtrl_Session_Catalyst6500::movePortToVLANAsTagged --> getVlanPortMapById (vlanPortMapListAll, vlan=", vlanID, ") failed.");
        return false;
    }

    vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
    if (vpmUntagged) {
         //bit==0 means port is untagged
        ResetBit(vpmUntagged->portbits, port_bit-1);
    } else {
        LOG(3)( Log::MPLS, "VLSR: SwitchCtrl_Session_Catalyst6500::movePortToVLANAsTagged --> getVlanPortMapById (vlanPortMapListUntagged, vlan=", vlanID, ") failed.");
        return false;
    }

    activeVlanId = vlanID;
    return ret;
}

//NOP!
bool SwitchCtrl_Session_Catalyst6500::setVLANPortsTagged(uint32 taggedPorts, uint32 vlanID)
{
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::removePortFromVLAN(uint32 port, uint32 vlanID)
{
    bool ret = true;
    vlanPortMap * vpmAll = NULL, *vpmUntagged = NULL;
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char type, value[500], oid_str[128], oct[3];
    int status, i;
    uint32 port_id, port_bit;
    portVlanMap vlanmap;
    uint8 mask;
    String tag_oid_str[4] = { ".1.3.6.1.4.1.9.9.46.1.6.1.1.4", ".1.3.6.1.4.1.9.9.46.1.6.1.1.17", \
    				".1.3.6.1.4.1.9.9.46.1.6.1.1.18", ".1.3.6.1.4.1.9.9.46.1.6.1.1.19"};

    if ((!active) || port==SWITCH_CTRL_PORT || vlanID<CATALYST6500_MIN_VLAN_ID || vlanID>CATALYST6500_MAX_VLAN_ID) 
    	return false; //don't touch the control port!
	
    port_id = hook_convertPortIDToInterface(port);
    if (port_id == 0)
    {
        LOG(2)( Log::MPLS, "VLSR: SNMP: Unknown port ", port);
        return false;
    }

    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        if (isPortTrunking(port)) {
            if (!cliSession.removePortFromVLAN(port, vlanID))
                return false;
        } else {
            if (!cliSession.removeUntaggedPortFromVLAN(port))
                return false;
        }
    }
    else // SNMP
    {
        // We only need to "remove" the port if the port is Trunkport   
        if (!isPortTrunking(port)) {
            // Set access VLAN ID to 1 (default)
            String tag_oid_str = ".1.3.6.1.4.1.9.9.68.1.2.2.1.2";
            sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
            type='i'; 
            sprintf(value, "%d", 1);
            if (!SNMPSet(oid_str, type, value)) 
            {
                LOG(3)( Log::MPLS, "VLSR: SNMP: Removing port ", port, " failed: cannot set access VLAN# to 1");
                return false; //turning off anyway
            }
            // Turn off the port
            SwitchPortOnOff(port, false); //Trun off the switch port
            goto _update_vpm;
        }
        
        if (!isSwitchport(port))
            return false;

        // Get the current vlan mapping for the port
        sprintf(oid_str, "%s.%d", tag_oid_str[(vlanID-1)/1024].chars(), port_id);
        status = read_objid(oid_str, anOID, &anOID_len);

        // Create the PDU for the data for our request.
        pdu = snmp_pdu_create(SNMP_MSG_GET);
        snmp_add_null_var(pdu, anOID, anOID_len);
        // Send the Request out.
        status = snmp_synch_response(snmpSessionHandle, pdu, &response);
        if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
        {
           vars = response->variables;
    	hook_getVlanMapFromSnmpVars(vlanmap, vars);
        	snmp_free_pdu(response);
        }
        else {
           if (status == STAT_SUCCESS){
              LOG(4)( Log::MPLS, "VLSR: SNMP: Reading Vlan map of Trunk port ", port, " failed. Reason : ", snmp_errstring(response->errstat));
           }
           else {
          	   snmp_sess_perror("snmpset", snmpSessionHandle);
              LOG(3)( Log::MPLS, "VLSR: SNMP: Reading Vlan map of Trunk port ", port, " failed with STAT_ERROR returned");
           }
           if(response) snmp_free_pdu(response);
           return false;
        }
       
        mask =(~(1<<(7-(vlanID%8)))) & 0xFF;
        vlanmap.vlanbits[vlanID/8 - ((uint32)(vlanID-1)/1024)*128] &= mask;

        // Set the vlan mapping for the port
        sprintf(oid_str, "%s.%d", tag_oid_str[(vlanID-1)/1024].chars(), port_id);
        value[0] = 0;
        for (i = 0; i < 128; i++) {
            snprintf(oct, 3, "%.2x", vlanmap.vlanbits[i]);
    	strcat(value,oct);
        }
        type='x';

        if (!SNMPSet(oid_str, type, value)) 
        {
           LOG(3)( Log::MPLS, "VLSR: SNMP: Setting Vlan map of Trunk port ", port, "failed.");
           return false;
        }
    }

_update_vpm:

    port_bit = convertUnifiedPort2Catalyst6500(port);
    if (vlanID>=CATALYST6500_MIN_VLAN_ID && vlanID<=CATALYST6500_MAX_VLAN_ID) {
       vpmAll = getVlanPortMapById(vlanPortMapListAll, vlanID);
       if (vpmAll) {
          ResetBit(vpmAll->portbits, port_bit-1);
          vpmUntagged = getVlanPortMapById(vlanPortMapListUntagged, vlanID);
          if (vpmUntagged) {
             //bit==1 means port is untagged
             SetBit(vpmUntagged->portbits, port_bit-1);
      	  }
    	  else
            return false;
       }
    } else {
        LOG(2) (Log::MPLS, "Trying to remove port from an invalid VLAN ", vlanID);
    }

    return ret;
}


/////////--------Catalyst 6500 specific functions------///////////


///////////------Catalyst 6500 specific hooks -------///////////

bool SwitchCtrl_Session_Catalyst6500::hook_createVLAN(const uint32 vlanID)
{
    char type, value[128], oid_str[128];

    if (!active) //not initialized or session has been disconnected
        return false;
    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        if (!cliSession.hook_createVLAN(vlanID))
            return false;
    }
    else // SNMP
    {
        String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
        sprintf(oid_str, "%s", tag_oid_str.chars());
        strcpy(value, "2");
        type='i'; 
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(3)( Log::MPLS, "VLSR: SNMP: Creating rows in the vtpVlanEditTable for the new VLAN ", vlanID, "failed.");
            return false;
        }

        // Create the VLAN using SNMP request
        tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.2.1.11.1";
        sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
        strcpy(value, "4");
        type='i'; 
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(3)( Log::MPLS, "VLSR: SNMP: Creating VLAN ", vlanID, "failed. ");
            return false;
        } 

        // Set the type of VLAN to 'ethernet' 
        tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.2.1.3.1";
        sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
        strcpy(value, "1");
        type='i'; 
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(3)( Log::MPLS, "VLSR: SNMP: Setting the type of VLAN ", vlanID, "to 'ethernet' failed. ");
            return false;
        } 

        // Apply the VLAN creation request 
        tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
        sprintf(oid_str, "%s", tag_oid_str.chars());
        strcpy(value, "3");
        type='i'; 
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(1)( Log::MPLS, "VLSR: SNMP: Applying the VLAN creation request failed.");
            return false;
        }

        // Release the Lock of the VLAN table 
        tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
        sprintf(oid_str, "%s", tag_oid_str.chars());
        strcpy(value, "4");
        type='i'; 
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(1)( Log::MPLS, "VLSR: SNMP: Releasing the Lock of the VLAN table creation failed.");
            return false;
        } 
   }

    //add the new *empty* vlan into PortMapListAll and portMapListUntagged
    vlanPortMap vpm;
    memset(&vpm, 0, sizeof(vlanPortMap));
    vpm.vid = vlanID;
    vlanPortMapListAll.push_back(vpm);
    memset(vpm.portbits, 0, MAX_VLAN_PORT_BYTES);
    vlanPortMapListUntagged.push_back(vpm);

    return true;
}

bool SwitchCtrl_Session_Catalyst6500::hook_removeVLAN(const uint32 vlanID)
{
    char type, value[128], oid_str[128];
    if (!active) //not initialized or session has been disconnected
        return false;

    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        if (!cliSession.hook_removeVLAN(vlanID))
            return false;
    }
    else // SNMP
    {
        // Create a rows in the vtpVlanEditTable for the new VLAN using SNMP request
        String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
        sprintf(oid_str, "%s", tag_oid_str.chars());
        strcpy(value, "2");
        type='i'; 
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(3)( Log::MPLS, "VLSR: SNMP: Locking the vtpVlanEditTable for editing to remove VLAN ", vlanID , "failed. ");
            return false;
        }


        // Delete the VLAN using SNMP request
        tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.2.1.11.1";
        sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
        strcpy(value, "6");
        type='i'; 
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(3)( Log::MPLS, "VLSR: SNMP: Removing VLAN ", vlanID , "failed. ");
            return false;
        }

        // Apply the VLAN removal  request 
        tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
        sprintf(oid_str, "%s", tag_oid_str.chars());
        strcpy(value, "3");
        type='i'; 
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(1)( Log::MPLS, "VLSR: SNMP: Applying the VLAN removal request failed.");
            return false;
        }
        
        // Release the Lock of the VLAN table 
        tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.4.1.1.1.1";
        sprintf(oid_str, "%s", tag_oid_str.chars());
        strcpy(value, "4");
        type='i'; 
        if (!SNMPSet(oid_str, type, value)) 
        {
            LOG(1)( Log::MPLS, "VLSR: SNMP: Releasing the Lock of the VLAN table after VLAN removal failed.");
            return false;
        } 
    }
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::hook_isVLANEmpty(const vlanPortMap &vpm)
{
    uint8 portbits[MAX_VLAN_PORT_BYTES];
    memset(portbits, 0, MAX_VLAN_PORT_BYTES);


    if ((CLI_SESSION_TYPE == CLI_TELNET || CLI_SESSION_TYPE == CLI_SSH) && strcmp(CLI_USERNAME, "unknown") != 0)
    {
        return cliSession.hook_isVLANEmpty(vpm);
    }
    return (memcmp(vpm.portbits, portbits, MAX_VLAN_PORT_BYTES) == 0);
}

void SwitchCtrl_Session_Catalyst6500::hook_getPortMapFromSnmpVars(vlanPortMap &vpm, netsnmp_variable_list *vars)
{
    memset(&vpm, 0, sizeof(vlanPortMap));
    if (vars->val.bitstring ){
        for (unsigned int i = 0; i < vars->val_len && i < MAX_VLAN_PORT_BYTES; i++) {
            vpm.portbits[i] = vars->val.bitstring[i];
       }
    }
    vpm.vid = (uint32)vars->name[vars->name_length - 1];
}

bool SwitchCtrl_Session_Catalyst6500::hook_hasPortinVlanPortMap(vlanPortMap &vpm, uint32  port)
{
    uint32 port_bit = convertUnifiedPort2Catalyst6500(port);
    if (port_bit == 0)
        return false;
    return HasPortBit(vpm.portbits, port_bit-1);
}

bool SwitchCtrl_Session_Catalyst6500::hook_getPortListbyVLAN(PortList& portList, uint32  vlanID)
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
            port = convertCatalyst65002UnifiedPort(port);
            if (port != 0)
                portList.push_back(port);
        }
    }

    if (portList.size() == 0)
        return false;
    return true;
}

bool SwitchCtrl_Session_Catalyst6500::verifyVLAN(uint32 vlanID)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    int status;
    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.3.1.1.2.1";

    if (!active || !snmp_enabled)
        return false;

    vlanID = hook_convertVLANIDToInterface(vlanID);

    if (vlanID == 0)
        return false;

    pdu = snmp_pdu_create(SNMP_MSG_GET);
    // vlan port list 
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), vlanID);
    status = read_objid(oid_str, anOID, &anOID_len);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out. 
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
            if (response->variables->type == 128 || response->variables->type == 129) { //NoSuchObject or NoSuchInstance
                snmp_free_pdu(response);
                return false;
            }     
            snmp_free_pdu(response);
            return true;
    }
    if(response) 
        snmp_free_pdu(response);
    return false;
}


//Note: By default the port number starts from 0 and slot number starts from 1. There may be cases that slot number starts from 0,
//for which we have to adjust the slot_id value to align with bitmap. (We may make this externally configurable if this variation is often seen.)
bool SwitchCtrl_Session_Catalyst6500::hook_createPortToIDRefTable(portRefIDList &portRefIdConvList)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    char ref_str[100];
    portRefID ref_id;
    size_t anOID_len = MAX_OID_LEN;
    int status;
    bool running = true;
    size_t rootlen;
    uint32 tmp_shelf_id = 0;
    uint32 tmp_slot_id = 0; 
    uint32 tmp_port_id = 100000; 

    if (!snmpEnabled())
        return false;

    status = read_objid(".1.3.6.1.2.1.2.2.1.2", anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));
    portRefIdConvList.clear();
    while (running) {
            // Create the PDU for the data for our request.
            pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
            pdu->non_repeaters = 0;
            pdu->max_repetitions = 100; 
            snmp_add_null_var(pdu, anOID, anOID_len);
            // Send the Request out.
            status = snmp_synch_response(snmpSessionHandle, pdu, &response);
            if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
                    for (vars = response->variables; vars; vars = vars->next_variable) {
                            if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                                    running = false;
                                    continue;
                            }

                            if (vars->val.string){
                                    strncpy(ref_str, (char*)vars->val.string, vars->val_len);
                                    ref_str[vars->val_len] = 0;
                            }
                            else
                                    ref_str[0] = 0;
                            if (ref_str[0] != 0) {
                                ref_id.ref_id = (int)vars->name[vars->name_length - 1];
                                tmp_port_id = 100000; 
                                tmp_slot_id = 0; 
                                if (sscanf(ref_str, "GigabitEthernet%d/0/%d", &tmp_slot_id, &tmp_port_id) == 2) {
                                  if (tmp_port_id>=CATALYST6500_MIN_PORT_ID && tmp_port_id<= CATALYST6500_MAX_PORT_ID ) {
	                                   ref_id.port_bit = ((tmp_slot_id-1)&0xf)*128 + (tmp_port_id&0xff);
	                                   tmp_slot_id += RSVP_Global::switchController->getSlotOffset(SLOT_TYPE_GIGE_OFFSET);
	                                   ref_id.port_id = ((tmp_shelf_id&0xf) << 12) | ((tmp_slot_id&0xf)<<8) | (tmp_port_id&0xff);
                                        	portRefIdConvList.push_back(ref_id);
        				    } else {
	                                   LOG(2) (Log::Error, "Illegal Port ID: ", tmp_port_id);
        				    }
                                }
                                else if (sscanf(ref_str, "GigabitEthernet%d/%d", &tmp_slot_id, &tmp_port_id) == 2) {
    				      if (tmp_port_id>=CATALYST6500_MIN_PORT_ID && tmp_port_id<= CATALYST6500_MAX_PORT_ID ) {
	                                   ref_id.port_bit = ((tmp_slot_id-1)&0xf)*128 + (tmp_port_id&0xff);
	                                   tmp_slot_id += RSVP_Global::switchController->getSlotOffset(SLOT_TYPE_GIGE_OFFSET);
	                                   ref_id.port_id = ((tmp_shelf_id&0xf) << 12) | ((tmp_slot_id&0xf)<<8) | (tmp_port_id&0xff);
	                                   portRefIdConvList.push_back(ref_id);
        				    } else {
	                                   LOG(2) (Log::Error, "Illegal Port ID: ", tmp_port_id);
        				    }
                                }
                                else if (sscanf(ref_str, "TenGigabitEthernet%d/0/%d", &tmp_slot_id, &tmp_port_id) == 2) {
    				      if (tmp_port_id>=CATALYST6500_MIN_PORT_ID && tmp_port_id<= CATALYST6500_MAX_PORT_ID ) {
	                                   ref_id.port_bit = ((tmp_slot_id-1)&0xf)*128 + (tmp_port_id&0xff);
	                                   tmp_slot_id += RSVP_Global::switchController->getSlotOffset(SLOT_TYPE_TENGIGE_OFFSET);
	                                   ref_id.port_id = ((tmp_shelf_id&0xf) << 12) | ((tmp_slot_id&0xf)<<8) | (tmp_port_id&0xff);
	                                   portRefIdConvList.push_back(ref_id);
        				    } else {
	                                   LOG(2) (Log::Error, "Illegal Port ID: ", tmp_port_id);
        				    }
                                }
                                else if (sscanf(ref_str, "TenGigabitEthernet%d/%d", &tmp_slot_id, &tmp_port_id) == 2) {
    				      if (tmp_port_id>=CATALYST6500_MIN_PORT_ID && tmp_port_id<= CATALYST6500_MAX_PORT_ID ) {
	                                   ref_id.port_bit = ((tmp_slot_id-1)&0xf)*128 + (tmp_port_id&0xff);
	                                   tmp_slot_id += RSVP_Global::switchController->getSlotOffset(SLOT_TYPE_TENGIGE_OFFSET);
	                                   ref_id.port_id = ((tmp_shelf_id&0xf) << 12) | ((tmp_slot_id&0xf)<<8) | (tmp_port_id&0xff);
	                                   portRefIdConvList.push_back(ref_id);
        				    } else {
            				    LOG(2) (Log::Error, "Illegal Port ID: ", tmp_port_id);
        				    }
                                }
                            }
                            if ((vars->type != SNMP_ENDOFMIBVIEW) &&
                                (vars->type != SNMP_NOSUCHOBJECT) &&
                                (vars->type != SNMP_NOSUCHINSTANCE)) {
                                memcpy((char *)anOID, (char *)vars->name, vars->name_length * sizeof(oid));
                                anOID_len = vars->name_length;
                            }
                            else {
                                running = false;
                            }
                    }
            }
            else {
                LOG(1)( Log::MPLS, "VLSR: SwitchCtrl_Session_Catalyst6500::hook_createPortToIDRefTable SNMP_GETBULK failed with STAT_ERROR returned");
                running = false;
            }
            if(response) snmp_free_pdu(response);
    }


    if (portRefIdConvList.size() == 0)
        return false;
    return true;
}

uint32 SwitchCtrl_Session_Catalyst6500::hook_convertPortInterfaceToID(uint32 id)
{
    portRefIDList::Iterator it;
    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
    {
        if ((*it).ref_id == id)
            return (*it).port_id;
    }
 
    return 0;
}

uint32 SwitchCtrl_Session_Catalyst6500::hook_convertPortIDToInterface(uint32 id)
{
    portRefIDList::Iterator it;
    for (it = portRefIdConvList.begin(); it != portRefIdConvList.end(); ++it)
    {
        if ((*it).port_id == id)
            return (*it).ref_id;
    }
 
    return 0;
}

// Need to change this function to use a different OID
bool SwitchCtrl_Session_Catalyst6500::hook_createVlanInterfaceToIDRefTable(vlanRefIDList &vlanRefIdConvList)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    char ref_str[100];
    vlanRefID ref_id;
    size_t anOID_len = MAX_OID_LEN;
    int status;
    bool running = true;
    size_t rootlen;
    uint32 tmp_vlan_id = 100000; 

    if (!snmpEnabled())
        return false;

    status = read_objid(".1.3.6.1.2.1.2.2.1.2", anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));
    vlanRefIdConvList.clear();
    while (running) {
            // Create the PDU for the data for our request.
            pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
            pdu->non_repeaters = 0;
            pdu->max_repetitions = 100; 
            snmp_add_null_var(pdu, anOID, anOID_len);
            // Send the Request out.
            status = snmp_synch_response(snmpSessionHandle, pdu, &response);
            if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) {
                    for (vars = response->variables; vars; vars = vars->next_variable) {
                            if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                                    running = false;
                                    continue;
                            }

                            if (vars->val.string){
                                    strncpy(ref_str, (char*)vars->val.string, vars->val_len);
                                    ref_str[vars->val_len] = 0;
                            }
                            else
                                    ref_str[0] = 0;
                            if (ref_str[0] != 0) {
                                ref_id.ref_id = (int)vars->name[vars->name_length - 1];
				tmp_vlan_id = 100000; 
                                if (sscanf(ref_str, "Vlan%d", &tmp_vlan_id) == 1) {
				    if (tmp_vlan_id>=CATALYST6500_MIN_VLAN_ID && tmp_vlan_id<= CATALYST6500_MAX_VLAN_ID) {
				    	ref_id.vlan_id = tmp_vlan_id;
                                    	vlanRefIdConvList.push_back(ref_id);
    				    } else {
        				LOG(2) (Log::Error, "Illegal VLAN ID ", tmp_vlan_id);
    				    }
				}
                            }
                            if ((vars->type != SNMP_ENDOFMIBVIEW) &&
                                (vars->type != SNMP_NOSUCHOBJECT) &&
                                (vars->type != SNMP_NOSUCHINSTANCE)) {
                                memcpy((char *)anOID, (char *)vars->name, vars->name_length * sizeof(oid));
                                anOID_len = vars->name_length;
                            }
                            else {
                                running = false;
                            }
                    }
            }
            else {
                LOG(1)( Log::MPLS, "VLSR: SwitchCtrl_Session_Catalyst6500::hook_createVlanInterfaceToIDRefTable SNMP_GETBULK failed with STAT_ERROR returned");
                running = false;
            }
            if(response) snmp_free_pdu(response);
    }


    if (vlanRefIdConvList.size() == 0)
        return false;
    return true;
}


bool SwitchCtrl_Session_Catalyst6500::readVlanPortMapListAllBranch(vlanPortMapList &vpmList) 
{
    portVlanMapList trunkPortVlanMapList;

    if (!snmp_enabled)
        return false;

    vpmList.clear();
    trunkPortVlanMapList.clear();

    readVlanPortMapBranch(".1.3.6.1.4.1.9.9.68.1.2.1.1.3", vpmList);
    readTrunkPortVlanMap(trunkPortVlanMapList);

    readVlanPortMapListALLFromPortVlanMapList(vpmList, trunkPortVlanMapList);
    return true;
}


bool SwitchCtrl_Session_Catalyst6500::readTrunkPortVlanMap(portVlanMapList &trunkPortVlanMapList)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    oid root[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    int status;
    portVlanMap vlanmap;
    bool running = true;
    size_t rootlen;

    status = read_objid(".1.3.6.1.4.1.9.9.46.1.6.1.1.4", anOID, &anOID_len);
    rootlen = anOID_len;
    memcpy(root, anOID, rootlen*sizeof(oid));
    while (running) {
        // Create the PDU for the data for our request.
        pdu = snmp_pdu_create(SNMP_MSG_GETBULK);
        pdu->non_repeaters = 0;
        pdu->max_repetitions = 100; 
        snmp_add_null_var(pdu, anOID, anOID_len);
        // Send the Request out.
        status = snmp_synch_response(snmpSessionHandle, pdu, &response);
        if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
	{
            for (vars = response->variables; vars; vars = vars->next_variable) 
	    {
                if ((vars->name_length < rootlen) || (memcmp(anOID, vars->name, rootlen * sizeof(oid)) != 0)) {
                    running = false;
                    continue;
                }

	 	int tmp_port_id = vars->name_loc[vars->name_length-1];
		//the port id could belong to a Port-Channel, which will be ignored since only gigaE and tenGigaE ports are counted.
		uint32 port = hook_convertPortInterfaceToID(tmp_port_id); // port = 0 for non-ethernet ports
		if (port > 0 && isPortTrunking(port)) {
			hook_getVlanMapFromSnmpVars(vlanmap, vars);
               	 	trunkPortVlanMapList.push_back(vlanmap);
		}

                if ((vars->type != SNMP_ENDOFMIBVIEW) &&
                    (vars->type != SNMP_NOSUCHOBJECT) &&
                    (vars->type != SNMP_NOSUCHINSTANCE)) {
                    memcpy((char *)anOID, (char *)vars->name, vars->name_length * sizeof(oid));
                    anOID_len = vars->name_length;
                }
                else {
                    running = false;
                }
            }
        }
        else {
            LOG(1)( Log::MPLS, "VLSR: SwitchCtrl_Session_Catalyst6500::readTrunkPortVlanMap SNMP_GETBULK failed with STAT_ERROR returned");
            running = false;
        }
        if(response) snmp_free_pdu(response);
    }
    return true;
}


void SwitchCtrl_Session_Catalyst6500::hook_getVlanMapFromSnmpVars(portVlanMap &pvm, netsnmp_variable_list *vars)
{
    memset(&pvm, 0, sizeof(portVlanMap));
    if (vars->val.bitstring ){
        for (unsigned int i = 0; i < vars->val_len && i < CATALYST_VLAN_BITLEN/8; i++) {
            pvm.vlanbits[i] = vars->val.bitstring[i];
       }
    }
     pvm.pid = (uint32)vars->name[vars->name_length - 1];
}


bool SwitchCtrl_Session_Catalyst6500::readVlanPortMapListALLFromPortVlanMapList(vlanPortMapList &vpmList, portVlanMapList &pvmList)
{
    uint32 portId, vlanId;
    int byteIndex, bitIndex;
    uint8 vlanbyte;

    // Iterate the List of Ports in the portVlanMapList
    portVlanMapList::Iterator pvmListIter = pvmList.begin();
    while (pvmListIter != pvmList.end()) 
    {
	portId = (*pvmListIter).pid;
	uint32 port_id = hook_convertPortInterfaceToID(portId);
	uint32 port_bit = convertUnifiedPort2Catalyst6500(port_id);
	if (port_bit == 0) {
		++pvmListIter;
		continue;
	}
	for (byteIndex=0; byteIndex<128; byteIndex++) {
	    vlanbyte = (uint8) (*pvmListIter).vlanbits[byteIndex]; 
	    bitIndex = 0;
	    while (vlanbyte) {
	   	if (vlanbyte >=128) { 
			vlanId = (byteIndex*8) + bitIndex;

			// Set the port 'port' for Vlan with 'vlanId' in the vlanPortMapList 'vpmList'
			vlanPortMapList::Iterator vpmListIter = vpmList.begin();
			while (vpmListIter != vpmList.end())
			{
			    if ((*vpmListIter).vid == vlanId) {
				    //port = convertUnifiedPort2Catalyst6500(port);
				    SetPortBit((*vpmListIter).portbits, port_bit-1);
				    //(*vpmListIter).portbits[port/8] = (*vpmListIter).portbits[port/8] + (uint8) exponentOf2((double)(port%8)-1);
			    	}
			    ++vpmListIter;
			}
		}
		vlanbyte <<= 1;
		bitIndex++;
	    }
	}
	++pvmListIter;
    }

    return true;
}

bool SwitchCtrl_Session_Catalyst6500::isPortTrunking(uint32 port)
{
    struct snmp_pdu *pdu;
    struct snmp_pdu *response;
    netsnmp_variable_list *vars;
    oid anOID[MAX_OID_LEN];
    size_t anOID_len = MAX_OID_LEN;
    char oid_str[128];
    int status;
    uint32 port_id;

    if (!active)  return false; 
    if (!isSwitchport(port)) return false;

    String tag_oid_str = ".1.3.6.1.4.1.9.9.46.1.6.1.1.13";
    port_id = hook_convertPortIDToInterface(port);
    sprintf(oid_str, "%s.%d", tag_oid_str.chars(), port_id);
    status = read_objid(oid_str, anOID, &anOID_len);

    // Create the PDU for the data for our request.
    pdu = snmp_pdu_create(SNMP_MSG_GET);
    snmp_add_null_var(pdu, anOID, anOID_len);
    // Send the Request out.
    status = snmp_synch_response(snmpSessionHandle, pdu, &response);
    if (status == STAT_SUCCESS && response->errstat == SNMP_ERR_NOERROR) 
    {
       if (response->variables->type == 128 || response->variables->type == 129) { //NoSuchObject or NoSuchInstance
           snmp_free_pdu(response);
           return false;
       }     
       vars = response->variables;
       bool ret = ((*(vars->val.integer)) ==1 || (*(vars->val.integer)) ==5);
    	snmp_free_pdu(response);
	return ret;
    }
    else {
       if (status == STAT_SUCCESS){
          LOG(6)( Log::MPLS, "VLSR: SNMP: Reading switchport ", port, " information at OID ",  oid_str, " failed. Reason : ", snmp_errstring(response->errstat));
       }
       else {
           LOG(1)( Log::MPLS, "VLSR: SwitchCtrl_Session_Catalyst6500::isPortTrunking SNMP_GET failed with STAT_ERROR returned");
      	    snmp_sess_perror("snmpset", snmpSessionHandle);
       }
       if(response) snmp_free_pdu(response);
       return false;
    }
   
    return false;
}




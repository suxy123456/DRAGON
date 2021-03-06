/****************************************************************************

  KOM RSVP Engine (release version 3.0f)
  Copyright (C) 1999-2004 Martin Karsten

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

  Contact:	Martin Karsten
		TU Darmstadt, FG KOM
		Merckstr. 25
		64283 Darmstadt
		Germany
		Martin.Karsten@KOM.tu-darmstadt.de

  Other copyrights might apply to parts of this package and are so
  noted when applicable. Please see file COPYRIGHT.other for details.

****************************************************************************/
%{
#include "RSVP_ConfigFileReader.h"
#include <stdio.h>
extern int yyparse();
extern int yylex();
extern int yylineno;
extern void yyerror(const char*);
extern FILE* yyin;
extern String yy_string;
extern uint32 yy_int;
extern ieee32float_p yy_float;
static String yy_ip_address;
static TimeValue yy_timeval;
static ConfigFileReader* cfr = NULL;
static const char* configfile = NULL;
static bool fatalError = false;
static String yy_ifType;
static String yy_host;
static String yy_swLayer;
%}

%token INTEGER FLOAT STRING IP_ADDRESS
%token INTERFACE API_C ROUTE REFRESH ENCAP VIRT DISABLE RAPID LOSS
%token TC_C NONE CBQ_C HFSC_C RATE PEER
%token TIMER SESSION_HASH API_HASH ID_HASH_SEND ID_HASH_RECV LIST_ALLOC SB_ALLOC
%token EXPLICIT_ROUTE_ MPLS_C NOMPLS MPLS_ALL NOMPLS_ALL LABEL_HASH LOCAL_ID UPSTREAM_LABEL_ INTEGER_RANGE
%token NARB SLOTS SLOT EXCLUDE NARB_EXTRA_OPTIONS NARB_VTAGS_ALLOWED
%token EOS_MAP VLAN_OPTIONS
%%

program:
	command program
	|
	;

command:
	INTERFACE name generic TC_C tc generic { cfr->createInterface(); cfr->cleanup(); }
	| INTERFACE name generic 	{ cfr->createInterface(); cfr->cleanup(); }
	| ROUTE dest mask gateway name		{ cfr->createRoute(); cfr->cleanup(); }
	| EXPLICIT_ROUTE_ route_hops		{ cfr->setExplicitRoute(); cfr->cleanup(); }
	| API_C INTEGER				{ cfr->setApiPort( yy_int ); }
	| TIMER tw_total tw_slots
	| SESSION_HASH INTEGER			{ cfr->setSessionHash(yy_int); }
	| API_HASH INTEGER			{ cfr->setApiHash(yy_int); }
	| ID_HASH_SEND INTEGER			{ cfr->setIdHashSend(yy_int); }
	| ID_HASH_RECV INTEGER			{ cfr->setIdHashRecv(yy_int); }
	| LIST_ALLOC INTEGER			{ cfr->setListAlloc(yy_int); }
	| SB_ALLOC INTEGER			{ cfr->setSB_Alloc(yy_int); }
	| MPLS_ALL				{ cfr->setGlobalMPLS( true ); cfr->mpls = true; }
	| NOMPLS_ALL				{ cfr->setGlobalMPLS( false ); cfr->mpls = false; }
	| LABEL_HASH INTEGER			{ cfr->setLabelHash(yy_int); }
	| PEER local_address remote_address INTEGER	{ cfr->addHop(yy_int); }
	| PEER local_address remote_address		{ cfr->addHop(); }
	| NARB narb_host narb_port	 	{ }
	| SLOTS if_type slot			{ }
	| EXCLUDE sw_layer excl_name		{ }
	| NARB_EXTRA_OPTIONS narb_option	{ }
	| NARB_VTAGS_ALLOWED vtags		{ }
	| EOS_MAP bandwidth STRING INTEGER	{ cfr->addEoSMap(yy_string, yy_int); }
	| VLAN_OPTIONS vlan_option		{ }
	;

/*** Addtions by Xi Yang ***/
narb_host:
	STRING		{ yy_host = yy_string; }
	| ip_address	{ yy_host = yy_ip_address; }
	;
narb_port:
	INTEGER		{ cfr->setNarbApiClient(yy_host, yy_int); }
	;
if_type:
	STRING		{ yy_ifType = yy_string; }
	;
slot:
	slot SLOT	{ cfr->addSlotInfo(yy_ifType, yy_string); }
	| slot INTEGER 	{ cfr->addSlotNum(yy_ifType, yy_int); }
	| SLOT	{ cfr->addSlotInfo(yy_ifType, yy_string); }
	| INTEGER	{ cfr->addSlotNum(yy_ifType, yy_int); }
	;
sw_layer:
	STRING		{ yy_swLayer = yy_string; }
	;
excl_name:
	excl_name STRING { cfr->addLayerExclusion(yy_swLayer, yy_string); }
	| STRING	 { cfr->addLayerExclusion(yy_swLayer, yy_string); }
	;
narb_option:
	narb_option STRING { cfr->setNarbExtraOption(yy_string); }
	| STRING	 { cfr->setNarbExtraOption(yy_string); }
	;
vtags:
	vtags INTEGER { cfr->setAllowedVtag(yy_int); }
	| vtags INTEGER_RANGE { cfr->setAllowedVtagRange(yy_string); }
	| INTEGER { cfr->setAllowedVtag(yy_int); }
	| INTEGER_RANGE { cfr->setAllowedVtagRange(yy_string); }
	;
vlan_option:
	vlan_option STRING { cfr->setSwitchVlanOption(yy_string); }
	| STRING	{ cfr->setSwitchVlanOption(yy_string); }
	;
/*** END: Addtions by Xi Yang ***/

tw_total:
	INTEGER		{ cfr->setTimerTotal(yy_int); }
        ;

tw_slots:
	INTEGER		{ cfr->setTimerSlots(yy_int); }
	;

generic:
	detail generic
	|
	;


detail:
	ENCAP local_port remote_address remote_port virt	{ cfr->encap = true; }
	| REFRESH INTEGER					{ cfr->refreshRate = yy_int; cfr->refresh = true; }
	| DISABLE						{ cfr->disable = true; }
	| RAPID INTEGER						{ cfr->rapidRefreshRate = yy_int; }
	| LOSS FLOAT						{ cfr->lossProb = yy_float; }
	| MPLS_C						{ cfr->mpls = true; }
	| NOMPLS						{ cfr->mpls = false; }
	| LOCAL_ID						{ cfr->localId = yy_string; }
	| UPSTREAM_LABEL_					{ cfr->upstreamLabel = yy_string; }
	;

tc:
	CBQ_C bandwidth latency		{ cfr->createTC_CBQ(); }
	| HFSC_C bandwidth latency		{ cfr->createTC_HFSC(); }
	| RATE bandwidth latency		{ cfr->createTC_Rate(); }
	| NONE 					{ cfr->createTC_NONE(); }
	;

name:
	STRING			{ cfr->interfaceName = yy_string; }
	;

local_port:
	INTEGER			{ cfr->localPort = yy_int; }
	;

local_address:
	ip_address		{ cfr->localAddress = yy_ip_address; }
	;

remote_address:
	ip_address		{ cfr->remoteAddress = yy_ip_address; }
	;

remote_port:
	remote_port INTEGER 	{ cfr->remotePortList.push_back( yy_int ); }
	| INTEGER	 	{ cfr->remotePortList.push_back( yy_int ); }
	;

virt:
	VIRT ip_address INTEGER	{ cfr->virtAddress = yy_ip_address; cfr->virtMTU = yy_int; cfr->virt = true; }
	|
	;

route_hops:
	route_hops ip_address	{ cfr->addExplicitRouteHop(yy_ip_address); }
	|  ip_address		{ cfr->addExplicitRouteHop(yy_ip_address); }
	;

bandwidth:
	INTEGER		{ cfr->bandwidth = yy_int; }
	| FLOAT		{ cfr->bandwidth = yy_float; }
	;

latency:
	INTEGER		{ cfr->latency = yy_int; }
	;

link_rate:
	RATE bandwidth
	|		{ cfr->bandwidth = 0; }
	;

dest:
	ip_address	{ cfr->dest = yy_ip_address; }
	;

mask:
	ip_address	{ cfr->mask = yy_ip_address; }
	;

gateway:
	ip_address	{ cfr->gateway = yy_ip_address; }
	;

timeval: 
	INTEGER		{ yy_timeval = TimeValue(yy_int,0); }
	| FLOAT		{ yy_timeval.getFromFraction(yy_float); }
	;

ip_address:
	IP_ADDRESS	{ yy_ip_address = yy_string; }
	| STRING	{ yy_ip_address = yy_string; }
	;

%%

extern void yyerror(const char *s) {
	cerr << s << " in config file: " << configfile << " on line " << yylineno << endl;
	ERROR(5)( Log::Error, s, "in config file:", configfile, "on line", yylineno );
	fatalError = true;
}

bool ConfigFileReader::parseConfigFile( const String& filename ) {
	cleanup();
	cfr = this;
	configfile = filename.chars();
	yyin = fopen( filename.chars(), "r" );
	if ( !yyin ) {
		cfr->warn( filename );
	} else {
		yyparse();
		fclose( yyin );
	}
	return !fatalError;
}

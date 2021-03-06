/*
 * OSPF Link State Advertisement
 * Copyright (C) 1999, 2000 Toshiaki Takada
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <zebra.h>

#include <zlib.h> /*for compress*/

#include "linklist.h"
#include "prefix.h"
#include "if.h"
#include "table.h"
#include "memory.h"
#include "stream.h"
#include "log.h"
#include "thread.h"
#include "hash.h"
#include "sockunion.h"		/* for inet_aton() */

#include "ospfd/ospfd.h"
#include "ospfd/ospf_te.h"
#include "ospfd/ospf_te_lsa.h"
#include "ospfd/ospf_te_lsdb.h"
#include "ospfd/ospf_interface.h"
#include "ospfd/ospf_ism.h"
#include "ospfd/ospf_asbr.h"
#include "ospfd/ospf_lsa.h"
#include "ospfd/ospf_lsdb.h"
#include "ospfd/ospf_neighbor.h"
#include "ospfd/ospf_nsm.h"
#include "ospfd/ospf_flood.h"
#include "ospfd/ospf_packet.h"
#include "ospfd/ospf_spf.h"
#include "ospfd/ospf_dump.h"
#include "ospfd/ospf_route.h"
#include "ospfd/ospf_ase.h"
#include "ospfd/ospf_zebra.h"
#include "ospfd/ospf_api.h"
#include "ospfd/ospf_apiserver.h"

#ifdef HAVE_OPAQUE_LSA

#define SET_LINK_PARAMS_LINK_HEADER_TLV(X) \
	  if (ntohs (para->X.header.type) != 0) \
	    length += TLV_SIZE (&para->X.header);

static void
set_linkparams_link_header (struct ospf_interface *oi)
{
  u_int16_t length = 0;
  struct te_area_lsa_para *para = &(oi->te_para);

  /* TE_LINK_SUBTLV_LINK_TYPE */
  SET_LINK_PARAMS_LINK_HEADER_TLV(link_type)
  
  /* TE_LINK_SUBTLV_LINK_ID */
  SET_LINK_PARAMS_LINK_HEADER_TLV(link_id)

  /* TE_LINK_SUBTLV_LCLIF_IPADDR */
  SET_LINK_PARAMS_LINK_HEADER_TLV(lclif_ipaddr)

  /* TE_LINK_SUBTLV_RMTIF_IPADDR */
  SET_LINK_PARAMS_LINK_HEADER_TLV(rmtif_ipaddr)

  /* TE_LINK_SUBTLV_TE_METRIC */
  SET_LINK_PARAMS_LINK_HEADER_TLV(te_metric)

  /* TE_LINK_SUBTLV_MAX_BW */
  SET_LINK_PARAMS_LINK_HEADER_TLV(max_bw)

  /* TE_LINK_SUBTLV_MAX_RSV_BW */
  SET_LINK_PARAMS_LINK_HEADER_TLV(max_rsv_bw)

  /* TE_LINK_SUBTLV_UNRSV_BW */
  SET_LINK_PARAMS_LINK_HEADER_TLV(unrsv_bw)

  /* TE_LINK_SUBTLV_RSC_CLSCLR */
  SET_LINK_PARAMS_LINK_HEADER_TLV(rsc_clsclr)

  /* The following are for GMPLS extensions */
  if (INTERFACE_GMPLS_ENABLED(oi)){
	  /* TE_LINK_SUBTLV_LCLRMT_ID */
	  SET_LINK_PARAMS_LINK_HEADER_TLV(link_lcrmt_id)
	  
	  /* TE_LINK_SUBTLV_PROTECTION_TYPE */
	  SET_LINK_PARAMS_LINK_HEADER_TLV(link_protype)
	  
	  /* TE_LINK_SUBTLV_IFSWCAP */
	  /*SET_LINK_PARAMS_LINK_HEADER_TLV(link_ifswcap)*/
	  if (para->link_ifswcap_list && listcount(para->link_ifswcap_list) > 0)
  	  {
		struct te_link_subtlv_link_ifswcap *ifswcap;
		listnode node;
		LIST_LOOP(para->link_ifswcap_list, ifswcap, node)
		{
			length += TLV_SIZE (&ifswcap->header);
		}
  	  }

	  /* TE_LINK_SUBTLV_IFSWCAP */
	  SET_LINK_PARAMS_LINK_HEADER_TLV(link_srlg)

	  /* TE_LINK_SUBTLV_TE_LAMBDA */
	  SET_LINK_PARAMS_LINK_HEADER_TLV(link_te_lambda)
  }
  

  para->link.header.type   = htons (TE_TLV_LINK);
  para->link.header.length = htons (length);

  return;
}

/*------------------------------------------------------------------------*
 * Followings are OSPF protocol processing functions for MPLS-TE.
 *------------------------------------------------------------------------*/

static void
build_tlv_header (struct stream *s, struct te_tlv_header *tlvh)
{
  stream_put (s, tlvh, sizeof (struct te_tlv_header));
  return;
}

/* The follow definitions declare functions for building link subtlvs */
#define FUNC_BUILD_LINK_SUBTLV(L) \
	static void \
	build_link_subtlv_ ## L  (struct stream *s, struct te_link_subtlv_ ## L *lp) \
	{ \
		struct te_tlv_header *tlvh = &lp->header; \
		if (ntohs(tlvh->type) != 0) \
		{ \
			build_tlv_header(s, tlvh); \
			stream_put(s, tlvh+1, TLV_BODY_SIZE(tlvh)); \
		} \
	   return; \
	}

FUNC_BUILD_LINK_SUBTLV(link_type)
FUNC_BUILD_LINK_SUBTLV(link_id)
FUNC_BUILD_LINK_SUBTLV(lclif_ipaddr)
FUNC_BUILD_LINK_SUBTLV(rmtif_ipaddr)
FUNC_BUILD_LINK_SUBTLV(te_metric)
FUNC_BUILD_LINK_SUBTLV(max_bw)
FUNC_BUILD_LINK_SUBTLV(max_rsv_bw)
FUNC_BUILD_LINK_SUBTLV(unrsv_bw)
FUNC_BUILD_LINK_SUBTLV(rsc_clsclr)
FUNC_BUILD_LINK_SUBTLV(link_lcrmt_id)
FUNC_BUILD_LINK_SUBTLV(link_protype)
/* FUNC_BUILD_LINK_SUBTLV(link_ifswcap_list) */
FUNC_BUILD_LINK_SUBTLV(link_te_lambda)

static int swcap_len_adjustment = 0;

static void 
build_link_subtlv_link_srlg (struct stream *s, struct te_link_subtlv_link_srlg *lp) 
{
	struct te_tlv_header *tlvh = &lp->header; 
	struct listnode *node;
	u_int32_t *v;
	
	if (ntohs(tlvh->type) != 0) 
	{ 
		build_tlv_header(s, tlvh); 
		LIST_LOOP(lp->srlg_list, v, node)
		{
			stream_put(s, v, sizeof(u_int32_t)); 
		}
	} 
	return; 
}

static void 
build_link_subtlv_link_ifswcap (struct stream *s, struct te_link_subtlv_link_ifswcap *lp) 
{
	struct te_tlv_header *tlvh = &lp->header; 
	uLongf z_len;

	swcap_len_adjustment = 0;

	if (ntohs(tlvh->type) != 0) 
	{ 
		struct te_tlv_header *tlvh_s = (struct te_tlv_header *)(s->data + s->putp); // pointing to the TLV header in stream
		build_tlv_header(s, tlvh); 

		stream_put(s, &lp->link_ifswcap_data.switching_cap, sizeof(u_char));
		stream_put(s, &lp->link_ifswcap_data.encoding, sizeof(u_char));
		/* Make sure reserved field is always zero */
		lp->link_ifswcap_data.reserved[0] = lp->link_ifswcap_data.reserved[1] = 0;
		stream_put(s, lp->link_ifswcap_data.reserved, sizeof(u_char)*2);
		stream_put(s, &lp->link_ifswcap_data.max_lsp_bw_at_priority, sizeof(float)*LINK_MAX_PRIORITY);
		
		if (lp->link_ifswcap_data.switching_cap >= LINK_IFSWCAP_SUBTLV_SWCAP_PSC1 &&
		    lp->link_ifswcap_data.switching_cap <= LINK_IFSWCAP_SUBTLV_SWCAP_PSC4)
		{
			stream_put(s, &lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_psc, sizeof(struct link_ifswcap_specific_psc));
		}
		else if (lp->link_ifswcap_data.switching_cap == LINK_IFSWCAP_SUBTLV_SWCAP_TDM)
		{
			if (ntohs(lp->header.length) == STD_ISCD_LENGTH)
			{
				stream_put(s, &lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_tdm, sizeof(struct link_ifswcap_specific_tdm));
			}
			else if (lp->link_ifswcap_data.encoding == LINK_IFSWCAP_SUBTLV_ENC_SONETSDH && ntohs(lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_SUBNET_UNI) 
			{ /*Ciena subnet UNI info process*/
				stream_put(s, &lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_subnet_uni,  sizeof(struct link_ifswcap_specific_subnet_uni));
			}
			else if (lp->link_ifswcap_data.encoding == LINK_IFSWCAP_SUBTLV_ENC_G709OTUK && ntohs(lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_ciena_otnx.version) & IFSWCAP_SPECIFIC_CIENA_OTNX) 
			{ /*Ciena OTNX process*/
				stream_put(s, &lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_ciena_otnx,  sizeof(struct link_ifswcap_specific_ciena_otnx));
			}
		}
		else if (lp->link_ifswcap_data.switching_cap == LINK_IFSWCAP_SUBTLV_SWCAP_LSC)
		{
			if (lp->link_ifswcap_data.encoding == LINK_IFSWCAP_SUBTLV_ENC_G709OCH && ntohs(lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_ciena_otnx.version) & IFSWCAP_SPECIFIC_CIENA_OTNX) 
			{ /*Ciena OTNX process*/
				stream_put(s, &lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_ciena_otnx,  sizeof(struct link_ifswcap_specific_ciena_otnx));
			}
		}
		else if (lp->link_ifswcap_data.switching_cap == LINK_IFSWCAP_SUBTLV_SWCAP_L2SC)
		{
			if ( (ntohs(lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_BASIC) &&
			  (ntohs(lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_ALLOC) )
			{
			    if ( !(ntohs(lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_COMPRESS_Z) ) {
			  	z_len = sizeof(struct link_ifswcap_specific_vlan) - 4;
				compress(z_buffer, &z_len, lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, z_len);
				assert(z_len <= sizeof(struct link_ifswcap_specific_vlan) - 4);
				lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version |= htons(IFSWCAP_SPECIFIC_VLAN_COMPRESS_Z);
				lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.length = htons(z_len + 4);

				/* Do not copy back to the oi->te_para, where the vlan tag mask remain uncompressed !*/
				/*memcpy(lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, z_buffer, z_len);*/
                
				/* change total TE link TLV length to indicate a compress operation*/
				swcap_len_adjustment = (z_len + 40) - ntohs(lp->header.length);
				lp->header.length = htons(z_len + 40);  /*adjust the TLV length*/
				tlvh_s->length = lp->header.length;

				stream_put(s, &lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan,  4); /* ifswcap_specific_vlan.length & version.*/
				stream_put(s, z_buffer, z_len); // ifswcap_specific_vlan.length & version.

				/* the structure itself is not compressed only the out going stream has compressed data */
				lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version &= ~(htons(IFSWCAP_SPECIFIC_VLAN_COMPRESS_Z));
				lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.length = htons(sizeof(struct link_ifswcap_specific_vlan));

			    }
			    else { /* compressed */
				/*stream_put(s, &lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan, sizeof(struct link_ifswcap_specific_vlan));*/
				stream_put(s, &lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan, ntohs(lp->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.length));
			    }
			}
		}
	} 
	return; 
}

static void 
build_link_subtlv_link_ifswcap_list (struct stream *s, struct ospf_interface* oi, struct te_tlv_header *tlvh_s)
{
  listnode node;
  struct te_link_subtlv_link_ifswcap *swcap;
  u_int32_t padding = 0;

  LIST_LOOP(oi->te_para.link_ifswcap_list, swcap, node)
  {
    build_link_subtlv_link_ifswcap(s, swcap);
    /* adjact header link TLV after compression */
    if ( (ntohs(swcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_BASIC) &&
    	(ntohs(swcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_ALLOC) )
    {
    	/* adjusting Link TLV length and including padding size for 4-byte alignment */
    	tlvh_s->length = htons(ntohs(tlvh_s->length) + swcap_len_adjustment);
    	if ((s->putp % 4) != 0)
    		stream_put(s, &padding, 4-(s->putp % 4));
    	if ((ntohs(tlvh_s->length) % 4) != 0)
    		tlvh_s->length = htons(ntohs(tlvh_s->length) + 4-(ntohs(tlvh_s->length) % 4));
    }
  }

  return;
}

/* The follow definitions are for calling functions defined above */
#define BUILD_LINK_SUBTLV(X)  build_link_subtlv_ ## X  (s, &oi->te_para. X)


static void
build_link_local_id_tlv (struct stream *s, struct ospf_interface *oi)
{
  struct te_tlv_link_local_id tlv;
  
  tlv.header.type = htons(TE_TLV_LINK_LOCAL_ID);
  tlv.header.length = htons(sizeof(u_int32_t));
  tlv.value = oi->te_para.link_lcrmt_id.link_local_id; /* link_local_id is already in network byte order */
  stream_put (s, &tlv, TLV_SIZE(&tlv.header));

  return;
}

/* 
  * Packing a list of GRI into the TE_LINK_SUBTLV_LINK_DRAGON_GRI TLV.
  * These GRI's in OSPF LSA updates will be associated with NARB resource queries in order to
  * eliminate double holdings. 
  */
static void
build_dragon_gri_tlv(struct stream* s, list gri_list)
{
  struct dragon_gri_para* gri;
  listnode node;
  struct timeval time_now;
  struct te_tlv_header tlv_header;
  struct te_tlv_header* tlvh_s = (struct te_tlv_header *)(s->data + s->putp); /* pointing to the TE Link sub-TLV header in stream*/
  int n_gri = 0;

  if (gri_list == NULL || listcount(gri_list) == 0)
    return;
  gettimeofday (&time_now, NULL);  
  tlv_header.type = htons(TE_LINK_SUBTLV_LINK_DRAGON_GRI);
  build_tlv_header(s, &tlv_header);
  LIST_LOOP(gri_list, gri, node)
  {
    if (time_now.tv_sec - gri->timestamp < MAX_GRI_AGE)
    {
        stream_putl(s, gri->ucid);
        stream_putl(s, gri->seqnum);
        n_gri++;
    }
  }
  if (n_gri == 0)
  {
    /*no tlv created*/
    s->putp -= sizeof(struct te_tlv_header);
    s->endp = s->putp;
    return;
  }
  tlvh_s->length = htons(n_gri*8);
}

static void
ospf_te_area_lsa_link_body_set (struct stream *s, struct ospf_interface *oi)
{
  /*
   * Only one Link TLV shall be carried in each LSA, allowing for fine
   * granularity changes in topology.
   */
  struct te_tlv_header *tlvh_s;

  set_linkparams_link_header (oi);
  tlvh_s = (struct te_tlv_header *)(s->data + s->putp); /* pointing to the TE Link TLV header in stream*/
  build_tlv_header (s, &oi->te_para.link.header);

  BUILD_LINK_SUBTLV(link_type);
  BUILD_LINK_SUBTLV(link_id);
  BUILD_LINK_SUBTLV(lclif_ipaddr);
  BUILD_LINK_SUBTLV(rmtif_ipaddr);
  BUILD_LINK_SUBTLV(te_metric);
  BUILD_LINK_SUBTLV(max_bw);
  BUILD_LINK_SUBTLV(max_rsv_bw);
  BUILD_LINK_SUBTLV(unrsv_bw);
  BUILD_LINK_SUBTLV(rsc_clsclr);

  if (INTERFACE_GMPLS_ENABLED(oi))
  {
	BUILD_LINK_SUBTLV(link_lcrmt_id);
	BUILD_LINK_SUBTLV(link_protype);
	build_link_subtlv_link_ifswcap_list(s, oi, tlvh_s);
	BUILD_LINK_SUBTLV(link_te_lambda);
	build_dragon_gri_tlv(s, oi->dragon_gri);
  }
  return;
}

static void
ospf_te_area_lsa_rtid_body_set (struct stream *s)
{
  /*
   * The router address TLV is type 1, and ...
   *                                      It must appear in exactly one
   * Traffic Engineering LSA originated by a router.
   */
   /*
   The Traffic
   Engineering (TE) LSA, which is an opaque LSA with area flooding scope
   [OSPF-TE], has only one top-level Type/Length/Value (TLV) triplet and
   has one or more nested sub-TLVs for extensibility.
   */
  struct te_tlv_header *tlvh = &OspfTeRouterAddr.header;
  if (ntohs (tlvh->type) != 0)
    {
      build_tlv_header (s, tlvh);
      stream_put (s, tlvh+1, TLV_BODY_SIZE (tlvh));
    }
  return;
}



static void
ospf_te_linklocal_lsa_body_set (struct stream *s, struct ospf_interface *oi)
{
  /*
   * Only one Link TLV shall be carried in each LSA, allowing for fine
   * granularity changes in topology.
   */
  build_link_local_id_tlv (s, oi);
  return;
}

static int
is_mandated_params_set_for_linklocaltlv(struct ospf_interface *oi)
{
  return (ntohs (oi->te_para.link_lcrmt_id.header.type) > 0);
}

static int
is_mandated_params_set_for_rtidtlv()
{
  return (ntohs (OspfTeRouterAddr.header.type) > 0);
}
static int
is_mandated_params_set_for_linktlv(struct ospf_interface *oi)
{
  int rc = 0;

  if (ntohs (OspfTeRouterAddr.header.type) == 0)
    goto out;

  if (ntohs (oi->te_para.link_type.header.type) == 0)
    goto out;

  if (ntohs (oi->te_para.link_id.header.type) == 0)
    goto out;

  rc = 1;
out:
  return rc;
}


struct ospf_lsa *
ospf_te_lsa_parse (struct ospf_lsa *new)
{
	u_int32_t lsa_id;
	u_char lsa_type;
	struct te_tlv_header *tlvh = NULL;
	struct te_tlv_header *sub_tlvh = NULL;
	u_int32_t read_len;
	/* struct te_link_subtlv_link_ifswcap* swcap; */
	/* uLongf z_len; */

	if (new == NULL)
		goto out;
	
	/* new->data->type is u_char, so ntohs(X) == X */
	if (new->data->type != OSPF_OPAQUE_AREA_LSA && 
	     new->data->type != OSPF_OPAQUE_LINK_LSA){
	     new->te_lsa_type = NOT_TE_LSA;  /* Just make sure this value is set to zero. */
	     new->tepara_ptr = NULL;
	     goto out;
	}
	lsa_type = GET_OPAQUE_TYPE(ntohl(new->data->id.s_addr));
	lsa_id = GET_OPAQUE_ID(ntohl(new->data->id.s_addr));
	if ((new->data->type == OSPF_OPAQUE_AREA_LSA && lsa_type != OPAQUE_TYPE_TE_AREA_LSA) ||
	     (new->data->type == OSPF_OPAQUE_LINK_LSA && (lsa_type != OPAQUE_TYPE_TE_LINKLOCAL_LSA ||
	                                                                                    lsa_id != 0)))
	{
		new->te_lsa_type = NOT_TE_LSA;
		new->tepara_ptr = NULL;
		goto out;
	}

	/* Otherwise, it is a TE-LSA, do rest of the parsing process */
	tlvh = TLV_HDR_TOP(new->data);
	if (new->tepara_ptr == NULL) 
		new->tepara_ptr = (struct te_lsa_para_ptr *) XMALLOC (MTYPE_OSPF_IF_PARAMS, sizeof(struct te_lsa_para_ptr));
  	memset (new->tepara_ptr, 0, sizeof(struct te_lsa_para_ptr));

	/* First, determine top-level tlv pointer */
	if (new->data->type == OSPF_OPAQUE_AREA_LSA && ntohs(tlvh->type) == TE_TLV_ROUTER_ADDR)
	{
		/* If this is a router ID LSA, then only the router address pointer needs to be filled */
		new->te_lsa_type = ROUTER_ID_TE_LSA;
		new->tepara_ptr->p_router_addr = (struct te_tlv_router_addr *)tlvh;
	}
	else if (new->data->type == OSPF_OPAQUE_LINK_LSA && ntohs(tlvh->type) == TE_TLV_LINK_LOCAL_ID)
	{
		/* If this is a link local ID LSA, then only the link local ID pointer needs to be filled */
		new->te_lsa_type = LINK_LOCAL_TE_LSA;
		new->tepara_ptr->p_link_local_id = (struct te_tlv_link_local_id *)tlvh;
	}
	else if (new->data->type == OSPF_OPAQUE_AREA_LSA && ntohs(tlvh->type) == TE_TLV_LINK)
	{
		new->te_lsa_type = LINK_TE_LSA;
		new->tepara_ptr->p_link = (struct te_tlv_link *)tlvh;
		/* Second, determine link sub-tlv pointers */
		read_len = 0;
		sub_tlvh = SUBTLV_HDR_TOP(tlvh);
		while (read_len < TLV_BODY_SIZE(tlvh))
		{
			switch (ntohs(sub_tlvh->type))
			{
				case TE_LINK_SUBTLV_LINK_TYPE:
					new->tepara_ptr->p_link_type = (struct te_link_subtlv_link_type *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_LINK_ID:
					new->tepara_ptr->p_link_id = (struct te_link_subtlv_link_id *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_LCLIF_IPADDR:
					new->tepara_ptr->p_lclif_ipaddr = (struct te_link_subtlv_lclif_ipaddr *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_RMTIF_IPADDR:
					new->tepara_ptr->p_rmtif_ipaddr = (struct te_link_subtlv_rmtif_ipaddr *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_TE_METRIC:
					new->tepara_ptr->p_te_metric = (struct te_link_subtlv_te_metric *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_MAX_BW:
					new->tepara_ptr->p_max_bw = (struct te_link_subtlv_max_bw *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_MAX_RSV_BW:
					new->tepara_ptr->p_max_rsv_bw = (struct te_link_subtlv_max_rsv_bw *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_UNRSV_BW:
					new->tepara_ptr->p_unrsv_bw = (struct te_link_subtlv_unrsv_bw *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_RSC_CLSCLR:
					new->tepara_ptr->p_rsc_clsclr = (struct te_link_subtlv_rsc_clsclr *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_LINK_LCRMT_ID:
					new->tepara_ptr->p_link_lcrmt_id = (struct te_link_subtlv_link_lcrmt_id *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_LINK_PROTYPE:
					new->tepara_ptr->p_link_protype = (struct te_link_subtlv_link_protype *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_LINK_IFSWCAP:
					if (!new->tepara_ptr->p_link_ifswcap_list)
						new->tepara_ptr->p_link_ifswcap_list = list_new();
					/*uncompress VLAN tag bitmask*/
					/*
					swcap = (struct te_link_subtlv_link_ifswcap *)sub_tlvh;
					if ((ntohs(swcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_BASIC) &&
					  (ntohs(swcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_ALLOC) &&
					  (ntohs(swcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version) & IFSWCAP_SPECIFIC_VLAN_COMPRESS_Z)) {
					  	z_len = sizeof(struct link_ifswcap_specific_vlan) - 4;
						uncompress(z_buffer, &z_len, swcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, ntohs(swcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.length) - 4);
						assert(z_len == sizeof(struct link_ifswcap_specific_vlan) - 4);
						memcpy(swcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.bitmask, z_buffer, z_len);
						swcap->link_ifswcap_data.ifswcap_specific_info.ifswcap_specific_vlan.version &= ~(htons(IFSWCAP_SPECIFIC_VLAN_COMPRESS_Z));
					}
					*/
					listnode_add(new->tepara_ptr->p_link_ifswcap_list, sub_tlvh);
					break;
				case TE_LINK_SUBTLV_LINK_SRLG:
					new->tepara_ptr->p_link_srlg = (struct te_tlv_header *)sub_tlvh;
					break;
				case TE_LINK_SUBTLV_LINK_TE_LAMBDA:
				        new->tepara_ptr->p_link_te_lambda = (struct te_link_subtlv_link_te_lambda *)sub_tlvh;
					break;
				default: /* Unrecognized link sub-tlv, just ignore it */
					break;
			}
			read_len += TLV_SIZE(sub_tlvh);
			sub_tlvh = SUBTLV_HDR_NEXT(sub_tlvh);
		}

		/* Check if mandatory sub-tlvs are included in this link tlv */
		if (new->tepara_ptr->p_link_type == NULL || new->tepara_ptr->p_link_id == NULL)
		{
			zlog_info ("ospf_te_lsa_parse: This TE-LSA lacks some mandatory TE parameters.");
			if (new->tepara_ptr->p_link_ifswcap_list != NULL)
				list_delete(new->tepara_ptr->p_link_ifswcap_list);
			XFREE(MTYPE_OSPF_IF_PARAMS, new->tepara_ptr);
			new->te_lsa_type = NOT_TE_LSA;
			new->tepara_ptr = NULL;
		}
	}
	else
	{
		zlog_info ("ospf_te_lsa_parse: Unrecognized TE-LSA due to incorrect TLV header info.");
		if (new->tepara_ptr->p_link_ifswcap_list != NULL)
			list_delete(new->tepara_ptr->p_link_ifswcap_list);
		XFREE(MTYPE_OSPF_IF_PARAMS, new->tepara_ptr);
		new->te_lsa_type = NOT_TE_LSA;
		new->tepara_ptr = NULL;
	}

out:
	return new;
}

static void 
ospf_te_lsa_check_content(struct ospf_lsa *new)
{
	struct ospf_area *area = new->area;
	struct listnode *node;
	struct ospf_interface *oi = NULL;
	u_char found;
	
	if ((!area) || listcount(area->oiflist)==0)
		return;
	

	if ( new->tepara_ptr->p_link_lcrmt_id )
	{
      		found = 0;
      		LIST_LOOP(area->oiflist, oi, node)
      		{
      			if (ntohs(oi->te_para.link_lcrmt_id.header.type) != 0 && 
      			     ntohl(new->tepara_ptr->p_link_lcrmt_id->link_remote_id) != 0 &&
      			     oi->te_para.link_lcrmt_id.link_local_id == new->tepara_ptr->p_link_lcrmt_id->link_remote_id)
      			{
	      			found = 1;
	      			break;
      			}
      		}
      		if (found)
      		{
      			if (ntohl(oi->te_para.link_lcrmt_id.link_remote_id) != 0 &&
      				oi->te_para.link_lcrmt_id.link_remote_id != new->tepara_ptr->p_link_lcrmt_id->link_local_id)
      				zlog_info ("ospf_te_lsa_check_content: configured remoted ID(%x)/received remote ID(%x) mismatch.", oi->te_para.link_lcrmt_id.link_remote_id, new->tepara_ptr->p_link_lcrmt_id->link_local_id);
      			else if (ntohl(oi->te_para.link_lcrmt_id.link_remote_id) == 0)
      				oi->te_para.link_lcrmt_id.link_remote_id = new->tepara_ptr->p_link_lcrmt_id->link_local_id;
      		}
	}
}

/* Install TE-LSA to an area. */
struct ospf_lsa *
ospf_te_lsa_install (struct ospf_lsa *new, struct ospf_interface *oi)
{
  struct ospf_area *area = new->area;
  struct ospf_te_lsdb *lsdb = NULL;
  struct ospf_lsa *old;
  int rt_recalc = 0;

  switch (new->te_lsa_type)
  {
  	case ROUTER_ID_TE_LSA:
  		lsdb = area->te_rtid_db;
  		break;
  	case LINK_TE_LSA:
  		lsdb = area->te_lsdb;
  		/* Verify this TE-LSA if it is for me */
	       if ((!IS_LSA_SELF(new)) &&
	           new->tepara_ptr->p_link_id->value.s_addr == OspfTeRouterAddr.value.s_addr)
  			ospf_te_lsa_check_content(new);
		if (IS_LSA_SELF(new) && new->oi != oi)
		{
			if (IS_DEBUG_OSPF (lsa, LSA_INSTALL))
				zlog_info("TE-LSA[Type%d] is self-orginated but its oi is %p. Change it to %p", new->data->type, new->oi, oi);				
			new->oi = oi;
		}
  		break;
  	case LINK_LOCAL_TE_LSA:
  		if ((!IS_LSA_SELF(new)) && INTERFACE_GMPLS_ENABLED(oi))
  		{
  		   if (ntohs(oi->te_para.link_lcrmt_id.header.type) != 0 &&
  		   	oi->te_para.link_lcrmt_id.link_remote_id != new->tepara_ptr->p_link_local_id->value)
  		   {
  		   	oi->te_para.link_lcrmt_id.link_remote_id = new->tepara_ptr->p_link_local_id->value;
  		   }
  		}
		if (IS_LSA_SELF(new) && new->oi != oi)
		{
			if (IS_DEBUG_OSPF (lsa, LSA_INSTALL))
				zlog_info("TE-LSA[Type%d] is self-orginated but its oi is %p. Change it to %p", new->data->type, new->oi, oi);				
			new->oi = oi;
		}
  		break;
  	default:
	 	zlog_info("Unrecoginzed TE-LSA[Type%d]: Opaque-ID %d", new->data->type, GET_OPAQUE_ID(ntohs (new->data->id.s_addr)));
  		return NULL;
  }
 if (lsdb != NULL)
 {
	old = ospf_te_lsdb_lookup(lsdb, new);

	if (old == NULL || ospf_lsa_different(old, new))
	 	rt_recalc = 1;

	/* discard old TE-LSA from TE-LSDB */
	if (old != NULL)
	    ospf_te_lsdb_delete(lsdb, old);

	/* Insert new TE-LSA into TE-LSDB */
	ospf_te_lsdb_add(lsdb, new);
	new->te_lsdb = lsdb;
 }
  /* Calculate Checksum if self-originated?. 
       This will be done in ospf_lsa_install 
  */

  /* The TE area routing table is recalculated, starting with
    * the shortest path calculation.
  */
  if (rt_recalc && area->router_lsa_self)
    ospf_te_cspf_calculate_schedule (area);

  if (IS_LSA_SELF (new) &&
	new->data->adv_router.s_addr == area->ospf->router_id.s_addr)
    {
      /* Set te-LSA refresh timer. */
      /* This is a regular timer */
      if (new->te_lsa_type == ROUTER_ID_TE_LSA)
      {
	      OSPF_TIMER_OFF (area->t_te_area_lsa_rtid_self);
	      OSPF_AREA_TIMER_ON (area->t_te_area_lsa_rtid_self,
				  ospf_te_area_lsa_rtid_timer, OSPF_LS_REFRESH_TIME);
	      /* Set self-originated te-area-LSA. */
	      ospf_lsa_unlock (area->te_area_lsa_rtid_self);
	      area->te_area_lsa_rtid_self = ospf_lsa_lock (new);
      		
      }
      else if (new->te_lsa_type == LINK_TE_LSA){
	      OSPF_TIMER_OFF (oi->t_te_area_lsa_link_self);
	      OSPF_INTERFACE_TIMER_ON (oi->t_te_area_lsa_link_self, ospf_te_area_lsa_link_timer, OSPF_LS_REFRESH_TIME);
	      
	      /* Set self-originated te-area-LSA. */
	      ospf_lsa_unlock (oi->te_area_lsa_link_self);
	      oi->te_area_lsa_link_self = ospf_lsa_lock (new);
      	}
      else  if (new->te_lsa_type == LINK_LOCAL_TE_LSA &&
      	            ntohs(oi->te_para.lclif_ipaddr.header.type) == 0){
	      OSPF_TIMER_OFF (oi->t_te_linklocal_lsa_self);
	      OSPF_INTERFACE_TIMER_ON (oi->t_te_linklocal_lsa_self, ospf_te_linklocal_lsa_timer, OSPF_LS_REFRESH_TIME);
	      
	      /* Set self-originated te-area-LSA. */
	      ospf_lsa_unlock (oi->te_linklocal_lsa_self);
	      oi->te_linklocal_lsa_self = ospf_lsa_lock (new);
      }

      if (IS_DEBUG_OSPF (lsa, LSA_INSTALL))
	zlog_info("TE-LSA[Type%d]: instance %x is self-originated",
		  new->data->type, GET_OPAQUE_ID(ntohs (new->data->id.s_addr)));
    }

  return new;
}

/* Create new TE-area LSA. */
static struct ospf_lsa *
ospf_te_area_lsa_rtid_new_for_area (struct ospf_area *area)
{
  struct stream *s;
  struct lsa_header *lsah;
  struct ospf_lsa *new = NULL;
  u_char options, lsa_type;
  struct in_addr lsa_id;
  u_int32_t tmp;
  u_int16_t length;

  /* Create a stream for LSA. */
  if ((s = stream_new (OSPF_MAX_LSA_SIZE)) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_rtid_new_for_area: stream_new() ?");
      goto out;
    }
  lsah = (struct lsa_header *) STREAM_DATA (s);

  options  = LSA_OPTIONS_GET (area);
#ifdef HAVE_NSSA
  options |= LSA_NSSA_GET (area);
#endif /* HAVE_NSSA */
  options |= OSPF_OPTION_O; /* Don't forget this :-) */

  lsa_type = OSPF_OPAQUE_AREA_LSA;
  /* instance = last 24 bits of the TE router address for this implementation */
  tmp = SET_OPAQUE_LSID (OPAQUE_TYPE_TE_AREA_LSA, ntohl(OspfTeRouterAddr.value.s_addr)); 
  lsa_id.s_addr = htonl (tmp);

  if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
    zlog_info ("LSA[Type%d:%s]: Create an Opaque-area router ID LSA/TE instance", lsa_type, inet_ntoa (lsa_id));

  /* Set opaque-LSA header fields. */
  lsa_header_set (s, options, lsa_type, lsa_id, area->ospf->router_id);

  /* Set opaque-LSA body fields. */
  ospf_te_area_lsa_rtid_body_set (s);

  /* Set length. */
  length = stream_get_endp (s);
  lsah->length = htons (length);

  /* Now, create an OSPF LSA instance. */
  if ((new = ospf_lsa_new ()) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_new_for_interface: ospf_lsa_new() ?");
      stream_free (s);
      goto out;
    }
  if ((new->data = ospf_lsa_data_new (length)) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_rtid_new_for_area: ospf_lsa_data_new() ?");
      ospf_lsa_free (new);
      new = NULL;
      stream_free (s);
      goto out;
    }

  new->area = area;

  SET_FLAG (new->flags, OSPF_LSA_SELF);
  memcpy (new->data, lsah, length);
  new = ospf_te_lsa_parse(new);
  
  stream_free (s);

out:
  return new;
}




static int
ospf_te_area_lsa_rtid_originate1 (struct ospf_area *area)
{
  struct ospf_lsa *new;
  int rc = -1;

  /* Create new Opaque-area LSA/TE instance. */
  if ((new = ospf_te_area_lsa_rtid_new_for_area(area)) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_link_originate1: ospf_te_area_lsa_link_new_for_interface() ?");
      goto out;
    }

  /* Install this LSA into both LSDB and TE-LSDB.
       That's why we need to call ospf_lsa_install() instead of ospf_te_lsa_install().
  */
  if (ospf_lsa_install (area->ospf, NULL, new) == NULL)	
    {
      zlog_warn ("ospf_te_area_lsa_rtid_originate1: ospf_lsa_install() ?");
      ospf_lsa_free (new);
      goto out;
    }

  /* Now this ospf interface has associated area LSA. 
    * The corresponding oi->ospf_te_area_lsa_self is updated during ospf_te_lsa_install()
    */
  

  /* Update new LSA origination count. */
  area->ospf->lsa_originate_count++;

  /* Flood new TE-area LSA through area. */
  ospf_flood_through_area (area, NULL/*nbr*/, new);

  if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
    {
      char area_id[INET_ADDRSTRLEN];
      strcpy (area_id, inet_ntoa (area->area_id));
      zlog_info ("LSA[Type%d:%x]: Originate TE-area LSA for Area(%s)", 
      	                 new->data->type, GET_OPAQUE_ID(ntohl(new->data->id.s_addr)), area_id);
      ospf_lsa_header_dump (new->data);
    }

  rc = 0;
out:
  return rc;
}

/* As soon as one interface is MPLS -enabled, this function should be called right away*/
/* If the router ID is changed, all TE-LSAs need to be re-originated */
int
ospf_te_area_lsa_rtid_originate (struct ospf_area *area)
{
  int rc = -1;

  if (!is_mandated_params_set_for_rtidtlv())
  {
       /*zlog_warn ("ospf_te_area_lsa_rtid_originate: TE router ID is not set.");*/
       goto out;
  }
  /* Ok, let's try to originate an LSA for this area and Link. */
  if (ospf_te_area_lsa_rtid_originate1 (area) != 0)
     goto out;
	
  rc = 0;
out:
  return rc;
}

int 
ospf_te_area_lsa_rtid_timer(struct thread *t)
{
   struct ospf_area *area = THREAD_ARG (t);
   int rc = -1;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[te-area-router-id-LSA]: (te-area-router-id-LSA Refresh expire)");

  area->t_te_area_lsa_rtid_self = NULL;
   
   if (area->te_area_lsa_rtid_self)
      rc = ospf_te_area_lsa_rtid_refresh(area->te_area_lsa_rtid_self);
   else
      rc = ospf_te_area_lsa_rtid_originate(area);
   return rc;
}


/* When timer on an ospf interface is triggered, this function may be called */
int
ospf_te_area_lsa_rtid_refresh (struct ospf_lsa *lsa)
{
  struct ospf_area *area = lsa->area;
  struct ospf_lsa *new = NULL;
  int rc = -1;
  
  if(!is_mandated_params_set_for_rtidtlv())
  {
	  if(IS_DEBUG_OSPF(lsa, LSA_REFRESH))
	       zlog_warn ("ospf_te_area_lsa_rtid_refresh: Link lacks some mandated  TE parameters.");
      goto out;
  }
  else if (!IPV4_ADDR_SAME (&lsa->data->adv_router, &OspfTeRouterAddr.value))
   {
	ospf_lsa_flush_area (lsa, area);
	ospf_lsa_unlock (area->te_area_lsa_rtid_self);
	area->te_area_lsa_rtid_self = NULL;
   } 
   else
   {
  	/* Delete LSA from neighbor retransmit-list. */
  	ospf_ls_retransmit_delete_nbr_area (area, lsa);
   }
   
  /* Create new Opaque-LSA/TE instance. */
  if ((new = ospf_te_area_lsa_rtid_new_for_area (area)) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_rtid_refresh: ospf_te_area_lsa_rtid_new_for_area() ?");
      goto out;
    }
  new->data->ls_seqnum = lsa_seqnum_increment (lsa);

  /* Install this LSA into both LSDB and TE-LSDB.
       That's why we need to call ospf_lsa_install() instead of ospf_te_lsa_install().
   */
  if (ospf_lsa_install (area->ospf, NULL, new ) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_rtid_refresh: ospf_lsa_install() ?");
      /*hacked@@@@*/
      if (new->lock == 0)
          ospf_lsa_free (new);
      goto out;
    }

  /* Flood updated LSA through area. */
  ospf_flood_through_area (area, NULL/*nbr*/, new);

  /* Debug logging. */
  if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
    {
      zlog_info ("LSA[Type%d:%d]: Refresh Opaque-LSA/MPLS-TE",
		 new->data->type, GET_OPAQUE_ID(ntohl(new->data->id.s_addr)));
      ospf_lsa_header_dump (new->data);
    }
  rc  = 0;
out:
  return rc;
}


/* Create new TE-area LSA. */
static struct ospf_lsa *
ospf_te_area_lsa_link_new_for_interface (struct ospf_interface *oi)
{
  struct stream *s;
  struct lsa_header *lsah;
  struct ospf_lsa *new = NULL;
  u_char options, lsa_type;
  struct in_addr lsa_id;
  u_int32_t tmp;
  u_int16_t length;
struct ospf_area *area = oi->area;

  if (!INTERFACE_MPLS_ENABLED(oi)) 
  	goto out;
  
  /* Create a stream for LSA. */
  if ((s = stream_new (OSPF_MAX_LSA_SIZE)) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_new_for_interface: stream_new() ?");
      goto out;
    }
  lsah = (struct lsa_header *) STREAM_DATA (s);

  options  = LSA_OPTIONS_GET (area);
#ifdef HAVE_NSSA
  options |= LSA_NSSA_GET (area);
#endif /* HAVE_NSSA */
  options |= OSPF_OPTION_O; /* Don't forget this :-) */

  lsa_type = OSPF_OPAQUE_AREA_LSA;
  tmp = SET_OPAQUE_LSID (OPAQUE_TYPE_TE_AREA_LSA, oi->ifp->ifindex); /* instance = ifindex */
  lsa_id.s_addr = htonl (tmp);

  if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
    zlog_info ("LSA[Type%d:%s]: Create an Opaque-area LSA/TE instance", lsa_type, inet_ntoa (lsa_id));

  /* Set opaque-LSA header fields. */
  lsa_header_set (s, options, lsa_type, lsa_id, area->ospf->router_id);

  /* Set opaque-LSA body fields. */
  ospf_te_area_lsa_link_body_set (s, oi);

  /* Set length. */
  length = stream_get_endp (s);
  lsah->length = htons (length);

  /* Now, create an OSPF LSA instance. */
  if ((new = ospf_lsa_new ()) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_new_for_interface: ospf_lsa_new() ?");
      stream_free (s);
      goto out;
    }
  if ((new->data = ospf_lsa_data_new (length)) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_new_for_interface: ospf_lsa_data_new() ?");
      ospf_lsa_free (new);
      new = NULL;
      stream_free (s);
      goto out;
    }

  new->area = area;
  new->oi = oi; 
  SET_FLAG (new->flags, OSPF_LSA_SELF);
  memcpy (new->data, lsah, length);
  new = ospf_te_lsa_parse(new);
  
  stream_free (s);

out:
  return new;
}


static int
ospf_te_area_lsa_link_originate1 (struct ospf_interface *oi)
{
  struct ospf_lsa *new;
  struct ospf_area *area = oi->area; 
  int rc = -1;

  /* Create new Opaque-area LSA/TE instance. */
  if ((new = ospf_te_area_lsa_link_new_for_interface(oi)) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_link_originate1: ospf_te_area_lsa_link_new_for_interface() ?");
      goto out;
    }

  /* Install this LSA into LSDB. */
  if (ospf_lsa_install (area->ospf, oi, new) == NULL)	
    {
      zlog_warn ("ospf_te_area_lsa_link_originate1: ospf_lsa_install() ?");
      ospf_lsa_free (new);
      goto out;
    }

  /* Now this ospf interface has associated area LSA. 
    * The corresponding oi->ospf_te_area_lsa_self is updated during ospf_te_lsa_install()
    */
  

  /* Update new LSA origination count. */
  area->ospf->lsa_originate_count++;

  /* Flood new TE-area LSA through area. */
  ospf_flood_through_area (area, NULL/*nbr*/, new);

  if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
    {
      char area_id[INET_ADDRSTRLEN];
      strcpy (area_id, inet_ntoa (area->area_id));
      zlog_info ("LSA[Type%d:%x]: Originate TE-area LSA for Area(%s), Link(%s)", 
      	                 new->data->type, GET_OPAQUE_ID(ntohl(new->data->id.s_addr)), area_id, oi->ifp->name);
      ospf_lsa_header_dump (new->data);
    }

  rc = 0;
out:
  return rc;
}

int
ospf_te_area_lsa_link_originate (struct ospf_interface *oi)
{
  int rc = -1;

  if (!INTERFACE_MPLS_ENABLED(oi))
  {
	zlog_info ("ospf_te_area_lsa_originate: TE is disabled now for interface %s.", oi->ifp->name);
	rc = 0; /* This is not an error case. */
	goto out;
  }
  if (!is_mandated_params_set_for_linktlv(oi))
  {
       /*zlog_warn ("ospf_te_lsa_originate: Link(%s) lacks some mandated TE parameters.", oi->ifp ? oi->ifp->name : "?");*/
       goto out;
  }
  /* Ok, let's try to originate an LSA for this area and Link. */
  if (ospf_te_area_lsa_link_originate1 (oi) != 0)
     goto out;
	
  rc = 0;
out:
  return rc;
}

int 
ospf_te_area_lsa_link_timer(struct thread *t)
{
   struct ospf_interface *oi = THREAD_ARG (t);
   int rc = -1;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[te-area-link-LSA]: (te-area-link-LSA Refresh expire)");

   oi->t_te_area_lsa_link_self = NULL;

   if (oi->te_area_lsa_link_self) {
       rc = ospf_te_area_lsa_link_refresh(oi->te_area_lsa_link_self);
     }
   else {
   	rc = ospf_te_area_lsa_link_originate(oi);
     }

   return rc;
}

/* When timer on an ospf interface is triggered, this function may be called */
int
ospf_te_area_lsa_link_refresh (struct ospf_lsa *lsa)
{
  struct ospf_area *area = lsa->area;
  struct ospf_interface *oi = lsa->oi;
  struct ospf_lsa *new = NULL;
  int rc = -1;
  
  if(!is_mandated_params_set_for_linktlv(oi))
  {
	  if(IS_DEBUG_OSPF(lsa, LSA_REFRESH))
	       zlog_warn ("ospf_te_area_lsa_link_refresh: Link lacks some mandated  TE parameters.");
      goto out;
  }
  if (!INTERFACE_MPLS_ENABLED(oi) )
    {
      /*
       * This LSA must have flushed before due to MPLS-TE status change.
       * It seems a slip among routers in the routing domain.
       */
      zlog_info ("ospf_te_area_lsa_link_refresh: TE is disabled for interface %s.", oi->ifp->name);
      oi->te_area_lsa_link_self = NULL;
      ospf_lsa_flush_area(lsa,area); /* This LSA will be flushed and removed from TE-DB eventually when maxage_lsa_remover() is called */
      goto out;
    }
   else if (!IPV4_ADDR_SAME (&lsa->data->adv_router, &OspfTeRouterAddr.value))
   {
	ospf_lsa_flush_area (lsa, area);
	ospf_lsa_unlock (oi->te_area_lsa_link_self);
	oi->te_area_lsa_link_self = NULL;
   } 
   else
   {
  	/* Delete LSA from neighbor retransmit-list. */
  	ospf_ls_retransmit_delete_nbr_area (area, lsa);
   }

  /* Create new Opaque-LSA/TE instance. */
  if ((new = ospf_te_area_lsa_link_new_for_interface (oi)) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_link_refresh: ospf_te_area_lsa_link_new_for_interface() ?");
      goto out;
    }
  new->data->ls_seqnum = lsa_seqnum_increment (lsa);

  /* Install this LSA into TE-LSDB. */
  /* Given "lsa" will be freed in the next function. */
  if (ospf_lsa_install (area->ospf, oi, new ) == NULL)
    {
      zlog_warn ("ospf_te_area_lsa_link_refresh: ospf_lsa_install() ?");
      ospf_lsa_free (new);
      goto out;
    }

  /* Flood updated LSA through area. */
  ospf_flood_through_area (area, NULL/*nbr*/, new);

  /* Debug logging. */
  if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
    {
      zlog_info ("LSA[Type%d:%d]: Refresh Opaque-LSA/MPLS-TE",
		 new->data->type, GET_OPAQUE_ID(ntohl(new->data->id.s_addr)));
      ospf_lsa_header_dump (new->data);
    }
  rc = 0;
out:
  return rc;
}

/* Create new TE-link-local LSA. */
static struct ospf_lsa *
ospf_te_linklocal_lsa_new_for_interface ( struct ospf_interface *oi)
{
  struct stream *s;
  struct lsa_header *lsah;
  struct ospf_lsa *new = NULL;
  u_char options, lsa_type;
  struct in_addr lsa_id;
  u_int32_t tmp;
  u_int16_t length;
  struct ospf_area *area = oi->area;

  if (!INTERFACE_GMPLS_ENABLED(oi)) 
  	goto out;
  
  /* Create a stream for LSA. */
  if ((s = stream_new (OSPF_MAX_LSA_SIZE)) == NULL)
    {
      zlog_warn ("ospf_te_link_local_lsa_new_for_interface: stream_new() ?");
      goto out;
    }
  lsah = (struct lsa_header *) STREAM_DATA (s);

  options  = LSA_OPTIONS_GET (area);
#ifdef HAVE_NSSA
  options |= LSA_NSSA_GET (area);
#endif /* HAVE_NSSA */
  options |= OSPF_OPTION_O; 

  lsa_type = OSPF_OPAQUE_LINK_LSA;
  tmp = SET_OPAQUE_LSID (OPAQUE_TYPE_TE_LINKLOCAL_LSA, 0); /* opaqueID must be zero */
  lsa_id.s_addr = htonl (tmp);

  if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
    zlog_info ("LSA[Type%d:%s]: Create an Opaque-Link local LSA/TE instance", lsa_type, inet_ntoa (lsa_id));

  /* Set opaque-LSA header fields. */
  lsa_header_set (s, options, lsa_type, lsa_id, area->ospf->router_id);

  /* Set opaque-LSA body fields. */
  ospf_te_linklocal_lsa_body_set (s, oi);

  /* Set length. */
  length = stream_get_endp (s);
  lsah->length = htons (length);

  /* Now, create an OSPF LSA instance. */
  if ((new = ospf_lsa_new ()) == NULL)
    {
      zlog_warn ("ospf_te_link_local_lsa_new_for_interface: ospf_lsa_new() ?");
      stream_free (s);
      goto out;
    }
  if ((new->data = ospf_lsa_data_new (length)) == NULL)
    {
      zlog_warn ("ospf_te_link_local_lsa_new_for_interface: ospf_lsa_data_new() ?");
      ospf_lsa_free (new);
      new = NULL;
      stream_free (s);
      goto out;
    }

  new->area = area;
  new->oi = oi;
  SET_FLAG (new->flags, OSPF_LSA_SELF);
  memcpy (new->data, lsah, length);
  new = ospf_te_lsa_parse(new);
  stream_free (s);

out:
  return new;
}

static int
ospf_te_linklocal_lsa_originate1 (struct ospf_interface *oi)
{
  struct ospf_lsa *new;
  struct ospf_area *area = oi->area; 
  int rc = -1;

  /* Create new Opaque-area LSA/TE instance. */
  if ((new = ospf_te_linklocal_lsa_new_for_interface(oi)) == NULL)
    {
      zlog_warn ("ospf_te_linklocal_lsa_originate1: ospf_te_linklocal_lsa_new_for_interface() ?");
      goto out;
    }

   
  if (ospf_lsa_install (area->ospf, oi, new ) == NULL)	
    {
      zlog_warn ("ospf_te_linklocal_lsa_originate1: ospf_lsa_install() ?");
      ospf_lsa_free (new);
      goto out;
    }
   
   
  /* Now this ospf interface has associated area LSA. 
    * The corresponding oi->ospf_te_linklocal_lsa_self is updated during ospf_te_lsa_install()
    */

  /* Update new LSA origination count. */
  area->ospf->lsa_originate_count++;

  /* Flood new TE-linklocal LSA through the interface ONLY. */
  ospf_flood_through_interface (oi, NULL/*nbr*/, new);

  if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
    {
      char area_id[INET_ADDRSTRLEN];
      strcpy (area_id, inet_ntoa (area->area_id));
      zlog_info ("LSA[Type%d:%x]: Originate TE-linklocal LSA for Area(%s), Link(%s)", 
      	                 new->data->type, GET_OPAQUE_ID(ntohl(new->data->id.s_addr)), area_id, oi->ifp->name);
      ospf_lsa_header_dump (new->data);
    }

  rc = 0;
out:
  return rc;
}

int
ospf_te_linklocal_lsa_originate (struct ospf_interface *oi)
{
  int rc = -1;

  if (!INTERFACE_GMPLS_ENABLED(oi))
  {
	zlog_info ("ospf_te_linklocal_lsa_originate: GMPLS-TE is disabled now for interface %s.", oi->ifp->name);
	rc = 0; /* This is not an error case. */
	goto out;
  }
  if (!is_mandated_params_set_for_linklocaltlv(oi))
  {
       /*zlog_warn ("ospf_te_lsa_originate: Link(%s) lacks some mandated TE parameters.", oi->ifp ? oi->ifp->name : "?");*/
       goto out;
  }
  /* Ok, let's try to originate an LSA for this area and Link. */
  if (ospf_te_linklocal_lsa_originate1 (oi) != 0)
     goto out;
	
  rc = 0;
out:
  return rc;
}

int 
ospf_te_linklocal_lsa_timer(struct thread *t)
{
   struct ospf_interface *oi = THREAD_ARG (t);
   int rc = -1;

  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("Timer[te-link-local-LSA]: (te-link-local-LSA Refresh expire)");

   oi->t_te_linklocal_lsa_self = NULL;

   if (oi->te_linklocal_lsa_self)
   	rc = ospf_te_linklocal_lsa_refresh(oi->te_linklocal_lsa_self);
   else
   	rc = ospf_te_linklocal_lsa_originate(oi);
   return rc;
}

/* When timer on an ospf interface is triggered, this function may be called */
int
ospf_te_linklocal_lsa_refresh (struct ospf_lsa *lsa)
{
  struct ospf_area *area = lsa->area;
  struct ospf_interface *oi = lsa->oi;
  struct ospf_lsa *new = NULL;
  int rc = -1;
  
  if(!is_mandated_params_set_for_linklocaltlv(oi))
  {
	  if(IS_DEBUG_OSPF(lsa, LSA_REFRESH))
	       zlog_warn ("ospf_te_linklocal_lsa_refresh: Link local identifier has not been set yet.");
      goto out;
  }
  if (!INTERFACE_GMPLS_ENABLED(oi) )
  {
      /*
       * This LSA must have flushed before due to MPLS-TE status change.
       * It seems a slip among routers in the routing domain.
       */
      zlog_info ("ospf_te_linklocal_lsa_refresh: GMPLS-TE is disabled for interface %s.", oi->ifp->name);
      oi->te_linklocal_lsa_self = NULL;
      ospf_lsa_flush_area(lsa,area);
      goto out;
  }
   else if (!IPV4_ADDR_SAME (&lsa->data->adv_router, &OspfTeRouterAddr.value))
   {
	ospf_lsa_flush_area (lsa, area);
	ospf_lsa_unlock (oi->te_linklocal_lsa_self);
	oi->te_linklocal_lsa_self = NULL;
   } 
   else
   {
	   /* Delete LSA from neighbor retransmit-list. */
	   ospf_ls_retransmit_delete_nbr_if (oi, lsa);
   }

  /* Create new Opaque-LSA/TE instance. */
  if ((new = ospf_te_linklocal_lsa_new_for_interface (oi)) == NULL)
    {
      zlog_warn ("ospf_te_linklocal_lsa_refresh: ospf_te_linklocal_lsa_new_for_interface() ?");
      goto out;
    }
  new->data->ls_seqnum = lsa_seqnum_increment (lsa);

  /* Do not install this LSA into TE-LSDB. */
  /* Given "lsa" will be freed in the next function. */
  if (ospf_lsa_install (area->ospf, oi, new) == NULL)
    {
      zlog_warn ("ospf_te_linklocal_lsa_refresh: ospf_lsa_install() ?");
      ospf_lsa_free (new);
      goto out;
    }

  /* Flood updated LSA through the interface ONLY. */
  ospf_flood_through_interface (oi, NULL/*nbr*/, new);

  /* Debug logging. */
  if (IS_DEBUG_OSPF (lsa, LSA_GENERATE))
    {
      zlog_info ("LSA[Type%d:%d]: Refresh Opaque-LSA/MPLS-TE",
		 new->data->type, GET_OPAQUE_ID(ntohl(new->data->id.s_addr)));
      ospf_lsa_header_dump (new->data);
    }
 rc = 0;
out:
  return rc;
}

void
ospf_te_cspf_calculate_schedule (struct ospf_area *area)
{
  /* 
     struct in_addr source_ip, dest_ip;
     list explicit_path = NULL;
     listnode node;   
     struct in_addr* nodedata;
  */
  if (IS_DEBUG_OSPF_EVENT)
    zlog_info ("CSPF: calculation timer is assumed to be scheduled");
}

#endif



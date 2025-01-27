/*
 * Software WAPI encryption implementation
 * Copyright (c) 2011, XRadioTech
 * Author: Janusz Dziedzic <janusz.dziedzic@tieto.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/netdevice.h>
#include <linux/types.h>
#include <linux/random.h>
#include <linux/compiler.h>
#include <linux/crc32.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <asm/unaligned.h>

#include <net/mac80211_xr.h>
#include "ieee80211_i.h"
#include "wapi.h"


static int ieee80211_wapi_decrypt(struct ieee80211_local *local,
				  struct sk_buff *skb,
				  struct ieee80211_key *key)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;
	int hdrlen = ieee80211_hdrlen(hdr->frame_control);
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	int data_len;

	if (!(status->flag & RX_FLAG_DECRYPTED)) {
		/* TODO - SMS4 decryption for firmware without
		 * SMS4 support */
		return RX_DROP_UNUSABLE;
	}


	data_len = skb->len - hdrlen - WAPI_IV_LEN - WAPI_ICV_LEN;
	if (data_len < 0)
		return RX_DROP_UNUSABLE;

	/* Trim ICV */
	skb_trim(skb, skb->len - WAPI_ICV_LEN);

	/* Remove IV */
	memmove(skb->data + WAPI_IV_LEN, skb->data, hdrlen);
	skb_pull(skb, WAPI_IV_LEN);

	return RX_CONTINUE;
}

ieee80211_rx_result
ieee80211_crypto_wapi_decrypt(struct ieee80211_rx_data *rx)
{
	struct sk_buff *skb = rx->skb;
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)skb->data;

	if (!ieee80211_is_data(hdr->frame_control))
		return RX_CONTINUE;

	if (ieee80211_wapi_decrypt(rx->local, rx->skb, rx->key))
		return RX_DROP_UNUSABLE;

	return RX_CONTINUE;
}

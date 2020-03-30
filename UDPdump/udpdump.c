/*
 * Copyright (c) 1999 - 2005 NetGroup, Politecnico di Torino (Italy)
 * Copyright (c) 2005 - 2006 CACE Technologies, Davis (California)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino, CACE Technologies 
 * nor the names of its contributors may be used to endorse or promote 
 * products derived from this software without specific prior written 
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef _MSC_VER
/*
 * we do not want the warnings about the old deprecated and unsecure CRT functions
 * since these examples can be compiled under *nix as well
 */
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "pcap.h"

typedef struct mac_header {
	u_char dest_addr[6];
	u_char src_addr[6];
	u_char type[2];
}mac_header;

/* 4 bytes IP address */
typedef struct ip_address
{
	u_char byte1;
	u_char byte2;
	u_char byte3;
	u_char byte4;
}ip_address;

/* IPv4 header */
typedef struct ip_header
{
	u_char	ver_ihl;		// Version (4 bits) + Internet header length (4 bits)
	u_char	tos;			// Type of service 
	u_short tlen;			// Total length 
	u_short identification; // Identification
	u_short flags_fo;		// Flags (3 bits) + Fragment offset (13 bits)
	u_char	ttl;			// Time to live
	u_char	proto;			// Protocol
	u_short crc;			// Header checksum
	u_char saddr[4];
	//ip_address saddr;
	u_char daddr[4];
	//ip_address daddr;
	u_int	op_pad;			// Option + Padding
}ip_header;

/* UDP header*/
typedef struct udp_header
{
	u_short sport;			// Source port
	u_short dport;			// Destination port
	u_short len;			// Datagram length
	u_short crc;			// Checksum
}udp_header;

/* prototype of the packet handler */
void packet_handler(u_char *param, const struct pcap_pkthdr *header, const u_char *pkt_data);



int main()
{
	pcap_if_t *alldevs;
	pcap_if_t *d;
	int inum;
	int i=0;
	pcap_t *adhandle;
	char errbuf[PCAP_ERRBUF_SIZE];
	u_int netmask;
	char packet_filter[] = "tcp port ftp";
	struct bpf_program fcode;
	
	/* Retrieve the device list */
	if(pcap_findalldevs(&alldevs, errbuf) == -1)
	{
		fprintf(stderr,"Error in pcap_findalldevs: %s\n", errbuf);
		exit(1);
	}
	
	/* Print the list */
	for(d=alldevs; d; d=d->next)
	{
		printf("%d. %s", ++i, d->name);
		if (d->description)
			printf(" (%s)\n", d->description);
		else
			printf(" (No description available)\n");
	}

	if(i==0)
	{
		printf("\nNo interfaces found! Make sure WinPcap is installed.\n");
		return -1;
	}
	
	printf("Enter the interface number (1-%d):",i);
	scanf("%d", &inum);
	
	/* Check if the user specified a valid adapter */
	if(inum < 1 || inum > i)
	{
		printf("\nAdapter number out of range.\n");
		
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return -1;
	}

	/* Jump to the selected adapter */
	for(d=alldevs, i=0; i< inum-1 ;d=d->next, i++);
	
	/* Open the adapter */
	if ((adhandle= pcap_open_live(d->name,	// name of the device
							 65536,			// portion of the packet to capture. 
											// 65536 grants that the whole packet will be captured on all the MACs.
							 1,				// promiscuous mode (nonzero means promiscuous)
							 1,			// read timeout
							 errbuf			// error buffer
							 )) == NULL)
	{
		fprintf(stderr,"\nUnable to open the adapter. %s is not supported by WinPcap\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return -1;
	}
	
	/* Check the link layer. We support only Ethernet for simplicity. */
	if(pcap_datalink(adhandle) != DLT_EN10MB)
	{
		fprintf(stderr,"\nThis program works only on Ethernet networks.\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return -1;
	}
	
	if(d->addresses != NULL)
		/* Retrieve the mask of the first address of the interface */
		netmask=((struct sockaddr_in *)(d->addresses->netmask))->sin_addr.S_un.S_addr;
	else
		/* If the interface is without addresses we suppose to be in a C class network */
		netmask=0xffffff; 
	

	//compile the filter
	if (pcap_compile(adhandle, &fcode, packet_filter, 1, netmask) <0 )
	{
		fprintf(stderr,"\nUnable to compile the packet filter. Check the syntax.\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return -1;
	}
	
	//set the filter
	if (pcap_setfilter(adhandle, &fcode)<0)
	{
		fprintf(stderr,"\nError setting the filter.\n");
		/* Free the device list */
		pcap_freealldevs(alldevs);
		return -1;
	}
	
	printf("\nlistening on %s...\n", d->description);
	
	/* At this point, we don't need any more the device list. Free it */
	pcap_freealldevs(alldevs);
	
	/* start the capture */
	pcap_loop(adhandle, 0, packet_handler, NULL);
	return 0;
}

/* Callback function invoked by libpcap for every incoming packet */
void packet_handler(u_char *param, const struct pcap_pkthdr *header, const u_char *pkt_data)
{
	struct tm *ltime;
	//char timestr[16];
	char timestr[21];
	mac_header *mh;
	ip_header *ih;
	udp_header *uh;
	int min, sec;
	static int seclast = 0;
	static int countrec = 0;
	static int countsend = 0;
	static u_char srcadd[100][4] = { 0 };
	static u_char dstadd[100][4] = { 0 };
	static int srcc[100] = { 0 };
	static int dstc[100] = { 0 };


	int length = sizeof(mac_header) + sizeof(ip_header);

	u_int ip_len;
	u_short sport,dport;
	time_t local_tv_sec;

	/*
	 * unused parameter
	 */
	(VOID)(param);
	mh = (mac_header*)pkt_data;


	/* convert the timestamp to readable format */
	local_tv_sec = header->ts.tv_sec;
	ltime=localtime(&local_tv_sec);
	//strftime( timestr, sizeof timestr, "%H:%M:%S", ltime);
	strftime(timestr, sizeof timestr, "%Y-%m-%d %I:%M:%S", ltime);
	


	sec = ltime->tm_sec;
	min = ltime->tm_min;
	
	

	/* print timestamp and length of the packet */
	//printf("%s.%.6d len:%d ", timestr, header->ts.tv_usec, header->len);
	

	/* retireve the position of the ip header */
	ih = (ip_header *) (pkt_data +
		14); //length of ethernet header

	/* retireve the position of the udp header */
	ip_len = (ih->ver_ihl & 0xf) * 4;
	uh = (udp_header *) ((u_char*)ih + ip_len);

	/* convert from network byte order to host byte order */
	sport = ntohs( uh->sport );
	dport = ntohs( uh->dport );

	/* print ip addresses and udp ports */
	/*printf("%d.%d.%d.%d.%d -> %d.%d.%d.%d.%d\n",
		ih->saddr.byte1,
		ih->saddr.byte2,
		ih->saddr.byte3,
		ih->saddr.byte4,
		sport,
		ih->daddr.byte1,
		ih->daddr.byte2,
		ih->daddr.byte3,
		ih->daddr.byte4,
		dport);*/
	u_char(*pft)[4] = NULL;
	u_char* ph = NULL;
	int req=-1;
	if (ih->daddr[0] == 192 && ih->daddr[1] == 168 && ih->daddr[2] == 1 && ih->daddr[3] == 108) {
		pft = &srcadd;
		ph = ih->saddr;
		req = 0;
		for (int i = 0; i < 100; i++) {
			if (0 == pft[i][0] && 0 == pft[i][1] && 0 == pft[i][2] && 0 == pft[i][3]) {
				pft[i][0] = ph[0];
				pft[i][1] = ph[1];
				pft[i][2] = ph[2];
				pft[i][3] = ph[3];
				srcc[i] += ntohs(ih->tlen);
				break;
			}
			if (ph[0] == pft[i][0] && ph[1] == pft[i][1] && ph[2] == pft[i][2] && ph[3] == pft[i][3]) {
				srcc[i] += ntohs(ih->tlen);
				break;
			}
		}
	}
	else if (ih->saddr[0] == 192 && ih->saddr[1] == 168 && ih->saddr[2] == 1 && ih->saddr[3] == 108) {
		pft = &dstadd;
		ph = ih->daddr;
		req = 1;

		for (int i = 0; i < 100; i++) {
			if (0 == pft[i][0] && 0 == pft[i][1] && 0 == pft[i][2] && 0 == pft[i][3]) {
				pft[i][0] = ph[0];
				pft[i][1] = ph[1];
				pft[i][2] = ph[2];
				pft[i][3] = ph[3];
				dstc[i] += ntohs(ih->tlen);
				break;
			}
			if (ph[0] == pft[i][0] && ph[1] == pft[i][1] && ph[2] == pft[i][2] && ph[3] == pft[i][3]) {
				dstc[i] += ntohs(ih->tlen);
				break;
			}
		}
	}
	else
		pft = NULL;
		
	static int count = 0;
	if (ih->daddr[0] == 192 && ih->daddr[1] == 168 && ih->daddr[2] == 1 && ih->daddr[3] == 109)
		countrec += ntohs(ih->tlen);
	if (ih->saddr[0] == 192 && ih->saddr[1] == 168 && ih->saddr[2] == 1 && ih->saddr[3] == 109)
		countsend += ntohs(ih->tlen);
	if (seclast == min) {
		
		;
		//printf("---%d---", count);
	}
	else {
		seclast = min;
		double spr = countrec * 1.0 / 1024;
		double sps = countsend * 1.0 / 1024;
		printf("rec---\033[46m%lf\033[0mKBps---\n", spr);
		printf("send---\033[46m%lf\033[0mKBps---\n", sps);
		countrec = 0;
		countsend = 0;
	}
	

		

	
	char Buff[110] = {0};
	FILE* fp;

	
	fopen_s(&fp, "D:/record.csv", "a+");
	char time[20];
	sprintf(time, "%4d-%2d-%2d %2d:%2d:%2d", ltime->tm_year+1900, ltime->tm_mon+1, ltime->tm_mday, ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
	char sradd[23];
	sprintf(sradd, "%02X::%02X::%02X::%02X::%02X::%02X", mh->src_addr[0], mh->src_addr[1], mh->src_addr[2], mh->src_addr[3], mh->src_addr[4], mh->src_addr[5]);
	char deadd[23];
	sprintf(deadd, "%02X::%02X::%02X::%02X::%02X::%02X", mh->dest_addr[0], mh->dest_addr[1], mh->dest_addr[2], mh->dest_addr[3], mh->dest_addr[4], mh->dest_addr[5]);
	char srid[16];
	sprintf(srid, "%d.%d.%d.%d", ih->saddr[0], ih->saddr[1], ih->saddr[2], ih->saddr[3]);
	char deid[16];
	sprintf(deid, "%d.%d.%d.%d", ih->daddr[0], ih->daddr[1], ih->daddr[2], ih->daddr[3]);

	static char user[100][20] = { 0 };
	static char pass[100][20] = { 0 };
	static char dead[100][20] = { 0 };
	static int sta[100] = { 0 };
	static int countuser = 0;
	
	printf("%s", Buff);
	char cop[100] = { 0 };
	//fwrite(&Buff, sizeof(Buff), 1, fp);
	char* p = cop;
	for (int i = 55; (i < header->caplen + 1); i++)
	{
		*p++ = pkt_data[i - 1];
	}
	int f = 0;
	static int passget = 0;
	if (req == 1) {
		sprintf(dead[0], "%d.%d.%d.%d", ih->daddr[0], ih->daddr[1], ih->daddr[2], ih->daddr[3]);
		if (cop[0] == 'U'&&cop[1] == 'S'&&cop[2] == 'E'&&cop[3] == 'R') {
			passget++;
			int tx = 0;
			for (; cop[tx] != 0x20; tx++);
			int ctx = tx+1;
			for (tx+=1; cop[tx] != 0x0d && cop[tx + 1] != 0x0a; tx++) {
				user[countuser][tx - ctx] = cop[tx];
			}
			user[countuser][tx - ctx] = '\0';
			printf("\n\033[42m%s\033[0m\n", user[countuser]);
		}
		if (cop[0] == 'P'&&cop[1] == 'A'&&cop[2] == 'S'&&cop[3] == 'S') {
			passget++;
			int tx = 0;
			for (; cop[tx] != 0x20; tx++);
			int ctx = tx + 1;
			for (tx += 1; cop[tx] != 0x0d && cop[tx + 1] != 0x0a; tx++) {
				pass[countuser][tx - ctx] = cop[tx];
			}
			pass[countuser][tx - ctx] = '\0';
			printf("\n\033[42m%s\033[0m\n", pass[countuser]);
		}
	}
	else if (req == 0) {
		if (cop[0] == '3'&&cop[1] == '3'&&cop[2] == '1'&&passget == 1) {
			passget++;
		}
		if (cop[0] == '5'&&cop[1] == '3'&&cop[2] == '0'&&passget == 3) {
			passget++;
			sta[countuser] = 0;
			printf("\n\033[42m%d\033[0m\n", sta[countuser]);
			printf("\n\033[41mFTP:%s\tUSER:%s\tPAS:%s\tSTA:%s\033[0m\n", dead[countuser], user[countuser], pass[countuser],
				sta[countuser] == 1 ? "OK" : "FAILED");
			sprintf(Buff,"FTP:,%s,USER:%s,PAS:%s,STA:%s\n", dead[countuser], user[countuser], pass[countuser],
				sta[countuser] == 1 ? "OK" : "FAILED");
			fwrite(Buff, 1, sizeof(Buff), fp);
		}
		if (cop[0] == '2'&&cop[1] == '3'&&cop[2] == '0'&&passget == 3) {
			passget++;
			sta[countuser] = 1;
			printf("\n\033[42m%d\033[0m\n", sta[countuser]);
			printf("\n\033[44mFTP:%s\tUSER:%s\tPAS:%s\tSTA:%s\033[0m\n", dead[countuser], user[countuser], pass[countuser],
				sta[countuser] == 1 ? "OK" : "FAILED");
			sprintf(Buff, "FTP:,%s,USER:%s,PAS:%s,STA:%s\n", dead[countuser], user[countuser], pass[countuser],
				sta[countuser] == 1 ? "OK" : "FAILED");
			fwrite(Buff, 1, sizeof(Buff), fp);
		}
	}

	if (passget == 4) {
		passget = 0;
	}

	//if (stop(sec, min) == 1) {
	//	for (int i = 0; i < 100; i++) {
	//		if (srcc[i] == 0)
	//			break;
	//		printf("%d----%d.%d.%d.%d--REC---%d\n", i, srcadd[i][0], srcadd[i][1], srcadd[i][2], srcadd[i][3], srcc[i]);
	//	}
	//	printf("\n\n");
	//	for (int i = 0; i < 100; i++) {
	//		if (dstc[i] == 0)
	//			break;
	//		printf("%d----%d.%d.%d.%d--SEN---%d\n", i, dstadd[i][0], dstadd[i][1], dstadd[i][2], dstadd[i][3], dstc[i]);
	//	}
	//	exit(0);
	//}
		
	fclose(fp);
	
}
int stop(int tsec,int tmin) {
	static int sec = -1;
	static int min = -1;
	if (sec == -1 && min == -1) {
		min = tmin;
		sec = tsec;
	}
	if (tsec == sec  && tmin == min+1) {
		return 1;
	}
	else
		return 0;
}
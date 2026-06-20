/*

Nintendont (Kernel) - Playing Gamecubes in Wii mode on a Wii U

Copyright (C) 2026  LittleCube

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include <sys/_types.h>

#include "global.h"
#include "common.h"
#include "alloc.h"
#include "sock.h"
#include "string.h"
#include "EXI.h"

#include "ap_tcp.h"
#include "sock.h"

#define SO_TOTAL_FDS 64

#define AF_INET 2
#define SOCK_STREAM 1
#define IPPROTO_IP 0

#define INADDR_ANY 0

#undef STACK_ALIGN
#define STACK_ALIGN(type, name, cnt, alignment) _Alignas(alignment) type name[cnt]

enum
{
	IOCTL_SO_ACCEPT = 0x1,
	IOCTL_SO_BIND = 0x2,
	IOCTL_SO_CLOSE = 0x3,
	IOCTL_SO_CONNECT = 0x4,
	IOCTL_SO_FCNTL = 0x5,
	IOCTL_SO_LISTEN = 0xA,
	IOCTL_SO_SOCKET = 0xF,
};

enum
{
	F_GETFL = 3,
	F_SETFL = 4,
};

enum
{
	O_NONBLOCK = 0x04,
};

enum
{
	ENOTCONN = -6,
	EWOULDBLOCK = -11,
};

typedef struct
{
	unsigned char sa_len;
	__sa_family_t sa_family;
	char sa_data[14];
} sockaddr;

typedef struct
{
	u8 len;
	u8 family;
	u16 port;
	u32 addr;
} wii_sockaddr_in;

typedef struct
{
	u32 socket;
	u32 has_name;
	u8 name[28];
} bind_params;

typedef struct
{
	void* data;
	u32 len;
} IoctlvData;

typedef struct
{
	u32 socket;
	u32 flags;
	u32 has_destaddr;
	u8 destaddr[8];
} sendto_params;

extern int sockFd;
extern int nwcFd;

int ap_tcp_init(APTCP* apt)
{
	int ncdFd = IOS_Open("/dev/net/ncd/manage", 0);
	
	apt->get_fcntl = 0;
	apt->waiting = 0;
	apt->accepted = 0;
	
	int status = -1;
	
	STACK_ALIGN(u32, in, 8, 32);
	STACK_ALIGN(u32, out, 8, 32);
	STACK_ALIGN(IoctlvData, v, 1, 32);
	
	v->data = out;
	v->len = 32;
	
	while (status != 0)
	{
		status = IOS_Ioctlv(ncdFd, 7, 0, 1, &v);
	}
	
	int i;
	
	for (i = 0; i < 5000; ++i)
	{
		out[0] = -1;
		status = IOS_Ioctl(nwcFd, 6, NULL, 0, out, sizeof(u32[8]));
		
		if (status >= 0)
		{
			break;
		}
		
		udelay(20);
	}
	
	IOS_Ioctl(nwcFd, 8, NULL, 0, out, 8);
	
	status = -1;
	
	for (i = 0; i < 5000; ++i)
	{
		status = IOS_Ioctl(sockFd, so_startinterface, NULL, 0, NULL, 0);
		
		if (status >= 0)
		{
			break;
		}
		
		udelay(20);
	}
	
	u32 ip;
	
	for (i = 0; i < 100; ++i)
	{
		ip = IOS_Ioctl(sockFd, 0x10, 0, 0, 0, 0);
		
		if (!ip)
		{
			udelay(100000);
		}
		
		else
		{
			break;
		}
	}
	
	in[0] = AF_INET;
	in[1] = SOCK_STREAM;
	in[2] = IPPROTO_IP;
	
	apt->sockfd = -1;
	
	for (i = 0; i < 5000; ++i)
	{
		status = apt->sockfd = IOS_Ioctl(sockFd, 0xF, in, sizeof(u32[3]), NULL, 0);
		
		if (apt->sockfd >= 0)
		{
			break;
		}
		
		udelay(20);
	}
	
	wii_sockaddr_in ap_addr = {0};
	ap_addr.len = sizeof(wii_sockaddr_in);
	ap_addr.family = AF_INET;
	ap_addr.port = 6969;
	ap_addr.addr = INADDR_ANY;
	
	STACK_ALIGN(bind_params, bind_args, 1, 32);
	bind_args->socket = apt->sockfd;
	bind_args->has_name = 1;
	memcpy(bind_args->name, &ap_addr, sizeof(wii_sockaddr_in));
	
	IOS_Ioctl(sockFd, IOCTL_SO_BIND, bind_args, sizeof(bind_params), NULL, 0);
	
	STACK_ALIGN(u32, sock_args, 4, 32);
	
	sock_args[0] = apt->sockfd;
	sock_args[1] = F_GETFL;
	sock_args[2] = 0;
	
	u32 sock_flags = IOS_Ioctl(sockFd, IOCTL_SO_FCNTL, sock_args, sizeof(u32[3]), NULL, 0);
	
	// does IOS_Ioctl overwrite sockfd at sock_args[0]?
	sock_args[0] = apt->sockfd;
	sock_args[1] = F_SETFL;
	sock_args[2] = sock_flags | O_NONBLOCK;
	
	IOS_Ioctl(sockFd, IOCTL_SO_FCNTL, sock_args, sizeof(u32[3]), NULL, 0);
	
	sock_args[0] = apt->sockfd;
	sock_args[1] = 1;
	
	IOS_Ioctl(sockFd, IOCTL_SO_LISTEN, sock_args, sizeof(u32[2]), NULL, 0);
	
	return apt->sockfd >= 0;
}

int ap_tcp_poll(APTCP* apt)
{
	//~ if (ioctl_ppc[0] != 0 && sock_entry_arm[0].busy == 0)
	//~ {
		//~ switch (ioctl_ppc[0])
		//~ {
			//~ case so_socket:
			//~ {
				//~ apt->sockfd = sock_entry_arm[0].retval;
				
				//~ sync_before_read((void*) ioctl_ppc, 0x40);
				//~ sync_before_read(&sock_entry_arm[0], 0x20);
				
				//~ ioctl_ppc[0] = so_bind;
				//~ ioctl_arm[0] = 0;
				//~ sock_entry_arm[0].busy = 1;
				//~ sock_entry_arm[0].cmd0 = apt->sockfd;
				
				//~ wii_sockaddr_in ap_addr = {0};
				//~ ap_addr.len = sizeof(wii_sockaddr_in);
				//~ ap_addr.family = AF_INET;
				//~ ap_addr.port = 6969;
				//~ ap_addr.addr = INADDR_ANY;
				
				//~ memcpy((void*) &sock_entry_arm[0].cmd1, &ap_addr, 8);
				//~ soBusy[0] = 0;
				
				//~ sync_after_write((void*) ioctl_ppc, 0x40);
				//~ sync_after_write(&sock_entry_arm[0], 0x20);
				
				//~ break;
			//~ }
			
			//~ case so_bind:
			//~ {
				//~ sync_before_read((void*) ioctl_ppc, 0x40);
				//~ sync_before_read((void*) &sock_entry_arm[0], 0x20);
				
				//~ ioctl_ppc[0] = so_fcntl;
				//~ ioctl_arm[0] = 0;
				//~ sock_entry_arm[0].busy = 1;
				
				//~ sock_entry_arm[0].cmd0 = apt->sockfd;
				//~ sock_entry_arm[0].cmd1 = F_GETFL;
				//~ sock_entry_arm[0].cmd2 = 0;
				
				//~ soBusy[0] = 0;
				
				//~ sync_after_write((void*) ioctl_ppc, 0x40);
				//~ sync_after_write((void*) &sock_entry_arm[0], 0x20);
				
				//~ break;
			//~ }
			
			//~ case so_fcntl:
			//~ {
				//~ if (!apt->get_fcntl)
				//~ {
					//~ u32 sock_flags = sock_entry_arm[0].retval;
					
					//~ sync_before_read((void*) ioctl_ppc, 0x40);
					//~ sync_before_read(&sock_entry_arm[0], 0x20);
					
					//~ ioctl_ppc[0] = so_fcntl;
					//~ ioctl_arm[0] = 0;
					//~ sock_entry_arm[0].busy = 1;
					
					//~ sock_entry_arm[0].cmd0 = apt->sockfd;
					//~ sock_entry_arm[0].cmd1 = F_SETFL;
					//~ sock_entry_arm[0].cmd2 = sock_flags | O_NONBLOCK;;
					
					//~ soBusy[0] = 0;
					
					//~ sync_after_write((void*) ioctl_ppc, 0x40);
					//~ sync_after_write(&sock_entry_arm[0], 0x20);
				//~ }
				
				//~ else
				//~ {
					//~ sync_before_read((void*) ioctl_ppc, 0x40);
					//~ sync_before_read(&sock_entry_arm[0], 0x20);
					
					//~ ioctl_ppc[0] = so_accept;
					//~ ioctl_arm[0] = 0;
					//~ sock_entry_arm[0].busy = 1;
					
					//~ sock_entry_arm[0].cmd0 = apt->sockfd;
					
					//~ soBusy[0] = 0;
					
					//~ sync_after_write((void*) ioctl_ppc, 0x40);
					//~ sync_after_write(&sock_entry_arm[0], 0x20);
					
					//~ apt->waiting = 1;
				//~ }
				
				//~ apt->get_fcntl = 1;
				
				//~ break;
			//~ }
			
			//~ case so_accept:
			//~ {
				//~ int status = sock_entry_arm[0].retval;
				
				//~ if (status >= 0)
				//~ {
					//~ sync_before_read((void*) ioctl_ppc, 0x40);
					//~ sync_before_read(&sock_entry_arm[0], 0x20);
					
					//~ ioctl_ppc[0] = so_accept;
					//~ ioctl_arm[0] = 0;
					//~ sock_entry_arm[0].busy = 1;
					
					//~ const char msg[] = "imagine archipelago LMFAO\n";
					
					//~ sock_entry_arm[0].cmd0 = apt->sockfd;
					//~ sock_entry_arm[0].cmd1 = (u32) msg;
					//~ sock_entry_arm[0].cmd2 = sizeof(msg);
					//~ sock_entry_arm[0].cmd3 = 0;
					//~ sock_entry_arm[0].cmd4 = 0;
					//~ sock_entry_arm[0].cmd5 = 0;
					
					//~ soBusy[0] = 0;
					
					//~ sync_after_write((void*) ioctl_ppc, 0x40);
					//~ sync_after_write(&sock_entry_arm[0], 0x20);
					
					//~ apt->accepted = 1;
				//~ }
				
				//~ else
				//~ {
					//~ sync_before_read((void*) ioctl_ppc, 0x40);
					//~ sync_before_read(&sock_entry_arm[0], 0x20);
					
					//~ ioctl_ppc[0] = so_accept;
					//~ ioctl_arm[0] = 0;
					//~ sock_entry_arm[0].busy = 1;
					
					//~ sock_entry_arm[0].cmd0 = apt->sockfd;
					
					//~ soBusy[0] = 0;
					
					//~ sync_after_write((void*) ioctl_ppc, 0x40);
					//~ sync_after_write(&sock_entry_arm[0], 0x20);
				//~ }
				
				//~ break;
			//~ }
		//~ }
		
		//~ ioctl_ppc[0] = 0;
	//~ }
	
	STACK_ALIGN(u32, sock_args, 4, 32);
	
	if (!apt->accepted)
	{
		sock_args[0] = apt->sockfd;
		
		s32 clientFd = IOS_Ioctl(sockFd, IOCTL_SO_ACCEPT, sock_args, sizeof(u32[1]), NULL, 0);
		
		if (clientFd >= 0)
		{
			const char* msg = "imagine archipelago LMFAO\n";
			
			STACK_ALIGN(IoctlvData, v, 2, 32);
			
			v[0].data = (void*) msg;
			v[0].len = strlen(msg) + 1;
			
			STACK_ALIGN(sendto_params, sp, 1, 32);
			
			sp->socket = clientFd;
			sp->flags = 0;
			sp->has_destaddr = 0;
			memset(&sp->destaddr, 0, 8);
			
			v[1].data = &sp;
			v[1].len = sizeof(sendto_params);
			
			IOS_Ioctlv(sockFd, 0xD, 2, 0, &v);
			
			apt->accepted = 1;
		}
		
		else if (clientFd == EWOULDBLOCK)
		{
			
		}
		
		else
		{
			
		}
	}
	
	return 1;
}

void ap_tcp_exit(APTCP* apt)
{
	//~ IOS_Ioctl(sockFd, so_shutdown, , 0, NULL, 0);
}
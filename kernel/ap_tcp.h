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
#ifndef _AP_TCP_H_
#define _AP_TCP_H_

typedef struct
{
	int sockfd;
	int get_fcntl;
	int waiting;
	int accepted;
} APTCP;

int ap_tcp_init(APTCP* apt);
int ap_tcp_poll(APTCP* apt);
void ap_tcp_exit(APTCP* apt);

#endif
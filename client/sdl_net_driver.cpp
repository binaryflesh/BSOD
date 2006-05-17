/*
Copyright (c) Sam Jansen and Jesse Baker.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. The end-user documentation included with the redistribution, if
any, must include the following acknowledgment: "This product includes
software developed by Sam Jansen and Jesse Baker 
(see http://www.wand.net.nz/~stj2/bung)."
Alternately, this acknowledgment may appear in the software itself, if
and wherever such third-party acknowledgments normally appear.

4. The hosted project names must not be used to endorse or promote
products derived from this software without prior written
permission. For written permission, please contact sam@wand.net.nz or
jtb5@cs.waikato.ac.nz.

5. Products derived from this software may not use the "Bung" name
nor may "Bung" appear in their names without prior written
permission of Sam Jansen or Jesse Baker.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL COLLABNET OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// $Id$
#include "stdafx.h"
#include "world.h"
#include "net_driver.h"
#include "exception.h"
#include "entity_manager.h"
#include "player.h"
#include "partflow.h"
#include "partvis.h"
#include "misc.h"

#include <SDL.h>
#include <SDL_net.h>


#ifdef WIN32
#pragma warning(disable:4005)
#include <WinSock2.h>
#else
#include <netinet/in.h>
#endif

#include "sdl_net_driver.h"

// DEBUG ONLY
//#define htonl(x) x
//#define ntohl(x) x
// END DEBUG ONLY

//#define htonf(x) x//(*(uint32*)&(x))
union
{
	float f;
	uint32 i;
} _u;
float htonf( float x )
{
	_u.f = (x);
	_u.i = htonl(_u.i);
	return( _u.f );
	//return( (float)(htonl( *((uint32*)&x) )) );
}
#define ntohf(x) htonf(x)

CNetDriver *CNetDriver::Create()
{
    return new CSDLNetDriver();
}
////////////////////////////////////////////////////////////////////////
CSDLNetDriver::CSDLNetDriver()
{
    // Assumes SDL_Init() has been called
    if(SDLNet_Init() == -1)
	throw CException(SDLNet_GetError());
	reconnect = false;
	first_connect = true;
	reconnect_wait = 500.0f;
}

CSDLNetDriver::~CSDLNetDriver()
{
    SDLNet_Quit();
}


void CSDLNetDriver::Connect(string address)
{
	this->address = address;
    string::size_type c = address.find(':', 0);
    string host = address.substr(0, c);
    string port = address.substr(c+1, string::npos);
    IPaddress ip;

    Log("Connecting to Host: '%s' Port: '%s'\n", host.c_str(), port.c_str());
    
    if(SDLNet_ResolveHost(&ip, (char *)host.c_str(), 
		(unsigned short)atoi(port.c_str())) == -1) {
        // Note: SDLNet_GetError() doesn't seem to say anything useful here.
	throw CException(bsprintf("Error: Unable to resolve host '%s'",
                    host.c_str()));
    }

    clientsock = SDLNet_TCP_Open(&ip);

    if(!clientsock)
	{
		if( first_connect )
		{
			throw CException(bsprintf("Unable to connect to server '%s': '%s'",
							host.c_str(), SDLNet_GetError()));
		}
		else
		{
			reconnect = true;
			return;
		}
	}
	first_connect = false;

    set = SDLNet_AllocSocketSet(16);
    if(!set)
	throw CException(SDLNet_GetError());

    if(SDLNet_TCP_AddSocket(set, clientsock) == -1)
	throw CException(SDLNet_GetError());
    
    Log("Connected!\n");

    char version;
    if(SDLNet_TCP_Recv(clientsock, &version, 1) != 1) {
	    Log("Unable to read server version\n");
	    throw CException("Unable to read server version\n");
    }

    /* Versions lower than 0x12 are ancient */
    if (version < 0x10) {
	    Log("Server version is ancient\n");
	    throw CException("Server version is ancient\n");
    }

    const int min_version = 0x14; // Minimum version known
    const int max_version = 0x14; // Maximum version known

    if (version < min_version) {
	    throw CException(bsprintf("Server version is %i.%i, need at least %i.%i\n",
			    version>>4,version&0x0F,
			    min_version>>4,max_version&0x0f));
    }

    // Maximum protocol version known is uh, 1.2
    if (version > max_version) {
	    throw CException(bsprintf("Server version is %i.%i, client version is only %i.%i\n",
			    version>>4,version&0x0F,
			    max_version>>4,max_version&0x0f));
    }

	reconnect = false;
    
}

void CSDLNetDriver::SendData(CPlayer *p)
{
    throw CException("CSDLNetDriver::SendData: This function is currently not"
	    " implemented!");
}

#ifdef _WIN32
#pragma pack(push)
#pragma pack(1)
#define PACKED
#else
#define PACKED __attribute__((packed))
#endif

struct flow_update_t {
    unsigned char type; // 0
    float x1;
    float y1;
    float z1;
    float x2;
    float y2;
    float z2;
    unsigned int id;
	uint32 ip1;
	uint32 ip2;
} PACKED;

struct pack_update_t {
    unsigned char type; // 1
    uint32 ts;
    unsigned int id;
    unsigned char id_num;
    unsigned short size;
    float speed;
	bool dark;
} PACKED;

struct flow_remove_t {
    unsigned char type; // 2
    unsigned int id;
} PACKED;

struct kill_all_t {
	bool all; // 3
} PACKED;

struct flow_descriptor
{
	unsigned char type;
	unsigned char id;
	byte colour[3];
	char name[256];
} PACKED;

union fp_union {
    struct flow_update_t flow;
    struct pack_update_t packet;
    struct flow_remove_t rem;
    struct kill_all_t kall;
	struct flow_descriptor fdesc;
};

#ifdef _WIN32
#pragma pack(pop)
#endif

//#define NET_DEBUG

void CSDLNetDriver::ReceiveData()
{
    byte buffer[1024];
    int  readlen;
    union fp_union *fp;


    while(SDLNet_CheckSockets(set, 0) > 0)
    {
		if((readlen = SDLNet_TCP_Recv(clientsock, buffer, 1024)) > 0)
		{
			int sam = (int)databuf.size();
			databuf.resize( sam + readlen );
			memcpy(&databuf[sam], buffer, readlen);

#ifdef NET_DEBUG
			Log("Read %d bytes\n", readlen);
#endif
		}
		else
		{
			// Disconnected or some unknown error try to reconnect:
			//Connect( address );
			reconnect = true;
			break;
		}
	}
	/*if(readlen == 0)
	    break;
    }*/

    // Log("Databuf.size()=%d\n", databuf.size());
    world.partVis->BeginUpdate();

	if( databuf.size() <= 0 )
		return;
    unsigned char *buf = &databuf[0];
    while( true ) 
    {
	const unsigned int s = (const unsigned int)(&databuf[databuf.size()-1] - &buf[0]);
	fp = (fp_union *)buf;
	if(s == 0) 
	{
	    databuf.erase(databuf.begin(), databuf.end());
	    break;
	}
	if(fp->flow.type == 0x00) 
	{
	    // Flow
	    if(sizeof(flow_update_t) <= s) 
	    {

#ifdef NET_DEBUG
			Log( "Flow: id=%u, x1=%f, y1=%f, z1=%f, x2=%f, y2=%f, z2=%f, ip1=%d, ip2=%d\n",
			ntohl(fp->flow.id),
			ntohf(fp->flow.x1),
			ntohf(fp->flow.y1),
			ntohf(fp->flow.z1),
			ntohf(fp->flow.x2),
			ntohf(fp->flow.y2),
			ntohf(fp->flow.z2),
			ntohl(fp->flow.ip1), 
			ntohl(fp->flow.ip2)
			);
#endif

		world.partVis->UpdateFlow(
			ntohl(fp->flow.id),
			Vector3f(ntohf(fp->flow.x1), ntohf(fp->flow.y1), ntohf(fp->flow.z1)),
			Vector3f(ntohf(fp->flow.x2), ntohf(fp->flow.y2), ntohf(fp->flow.z2)),
			ntohl( fp->flow.ip1 ), ntohl( fp->flow.ip2 ));

		buf += sizeof(flow_update_t);
	    } 
	    else 
	    {
		if(buf != &databuf[0])
		    databuf.erase(databuf.begin(), databuf.begin() + (buf - &databuf[0]));
		break;
	    }
	} 
	else if(fp->flow.type == 0x01) 
	{
	    // Packet
		//Log( "PACKET!\n" );
	    if(sizeof(pack_update_t) <= s) {

#ifdef NET_DEBUG
			Log( "Packet: flow=%u, ts=%u, id_num=%u, size=%u, speed=%f, dark=%d\n",
			ntohl(fp->packet.id), 
			ntohl(fp->packet.ts),
			fp->packet.id_num,
			fp->packet.size,
			ntohf(fp->packet.speed),
			fp->packet.dark
			);
#endif
			
			world.partVis->UpdatePacket(
							ntohl(fp->packet.id), 
							ntohl(fp->packet.ts),
							fp->packet.id_num,
							fp->packet.size,
							ntohf(fp->packet.speed),
							fp->packet.dark);

			buf += sizeof(pack_update_t);
	    } 
	    else 
	    {
		if(buf != &databuf[0])
		    databuf.erase(databuf.begin(), databuf.begin() 
			    + (buf - &databuf[0]));
		break;
	    }
	} 
	else if(fp->flow.type == 0x02) 
	{
#ifdef NET_DEBUG
		Log( "Kill: flow=%u\n", ntohl(fp->rem.id) );
#endif

	   if(sizeof(flow_remove_t) <= s) 
	    {
			world.partVis->RemoveFlow(ntohl(fp->rem.id));

			buf += sizeof(flow_remove_t);
	    } 
	    else 
	    {
			if(buf != &databuf[0])
				databuf.erase(databuf.begin(), databuf.begin() 
					+ (buf - &databuf[0]));
			break;
	    }
	}
	else if( fp->flow.type == 0x03 )
	{
#ifdef NET_DEBUG
		Log( "Kill all flows!\n" );
#endif
	    // Kill all existing flows!
	    world.partVis->KillAll();
	    buf += sizeof(kill_all_t);
	}
	else if( fp->flow.type == 0x04 )
	{
		// Flow descriptor.
		// Check if we have it already, if we don't add it, if we do
		// update it if its different, else do nothing.
		//int tempK = fp->fdesc.id;
		//int ppK = tempK;

#ifdef NET_DEBUG
		Log( "Flow descriptor\n" );
#endif

		if( world.partVis->fdmap.find( fp->fdesc.id ) == world.partVis->fdmap.end() )
		{
			// Log( "Adding new fd.\n" );
			// Not in the map yet so lets add it:
			FlowDescriptor *fd = new FlowDescriptor();
			fd->colour[0] = fp->fdesc.colour[0];
			fd->colour[1] = fp->fdesc.colour[1];
			fd->colour[2] = fp->fdesc.colour[2];
			fd->show = true; // Initially show this.
			strcpy( fd->name, fp->fdesc.name );
			//Log( "New flow desc: %s %d %d %d ID: %d", fd->name, fd->colour[0], fd->colour[1], fd->colour[2], fp->fdesc.id );
			world.partVis->fdmap.insert( FlowDescMap::value_type(fp->fdesc.id, fd) );

			if( (fd->colour[0] == 0 ) && (fd->colour[1] == 0 ) && ( fd->colour[2] == 0 ) )
				world.partVis->pGui->InitFD();
		}
		buf += sizeof(flow_descriptor);
	}
	else 
	{
	    Log("Unknown packet type... type=%d  || s:%u r:%u d:%u \n",
			fp->flow.type,
		    databuf.size(), 
		    (int)(buf-&databuf[0]), 
		    (int)(&databuf[databuf.size()-1]-buf)
	       );
	    databuf.erase(databuf.begin(), databuf.end());
	    break;
	}
    }

    world.partVis->EndUpdate();
}

void CSDLNetDriver::Reconnect()
{
	world.partVis->KillAll();
	
	world.partVis->fdmap.clear();
	world.partVis->pGui->InitFD();


	Connect( address );
	if( reconnect ) // Still not connected. Wait longer next time.
	{
		if( reconnect_wait < 120000.0f ) // Don't wait longer than 2 minutes between connection attempts.
			reconnect_wait *= 2.0f;
	}
	else // Connected so reset the wait time.
	{
		reconnect_wait = 500.0f;

		// Also, need to clear the flow desriptors since we will get a new list.
		// (if we keep the old list we get problems if the server was changed and is sending a different fd list).
		databuf.erase(databuf.begin(), databuf.end());
	}
}

bool CSDLNetDriver::Reconnecting()
{
	return( reconnect );
}

float CSDLNetDriver::WaitTime()
{
	return( reconnect_wait );
}

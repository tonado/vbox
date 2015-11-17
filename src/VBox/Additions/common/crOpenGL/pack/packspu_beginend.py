# Copyright (c) 2001, Stanford University
# All rights reserved.
#
# See the file LICENSE.txt for information on redistributing this software.

import sys

import apiutil


apiutil.CopyrightC()

print """/* DO NOT EDIT - AUTOMATICALLY GENERATED BY packspu_beginend.py */
#include "packspu.h"
#include "assert.h"
#include "cr_packfunctions.h"
#include "packspu_proto.h"

void PACKSPU_APIENTRY packspu_Begin( GLenum mode )
{
    GET_THREAD(thread);
    CRPackBuffer *buf = &thread->BeginEndBuffer;

    /* XXX comparing mode >= 0 here is not needed since mode is unsigned */
    CRASSERT( /*mode >= GL_POINTS && */mode <= GL_POLYGON );

#if CR_ARB_vertex_buffer_object
    {
        GLboolean serverArrays = GL_FALSE;
        GET_CONTEXT(ctx);
        if (ctx->clientState->extensions.ARB_vertex_buffer_object)
            serverArrays = crStateUseServerArrays();
        if (serverArrays) {
            CRClientState *clientState = &(ctx->clientState->client);
            if (clientState->array.locked && !clientState->array.synced)
            {
                crPackLockArraysEXT(clientState->array.lockFirst, clientState->array.lockCount);
                clientState->array.synced = GL_TRUE;
            }
        }
    }
#endif

    if (pack_spu.swap)
    {
        crPackBeginSWAP( mode );
    }
    else
    {
        crPackBegin( mode );
    }

    if ( thread->netServer.conn->Barf ) {
        thread->BeginEndMode = mode;
        thread->BeginEndState = -1;
        if ( mode == GL_LINES || mode == GL_TRIANGLES || mode == GL_QUADS || mode == GL_POLYGON )
        {
            CRASSERT(!buf->pack);

            crPackReleaseBuffer( thread->packer );
            buf->pack = crNetAlloc( thread->netServer.conn );
            crPackInitBuffer( buf, buf->pack, thread->netServer.conn->buffer_size, thread->netServer.conn->mtu );
            buf->holds_BeginEnd = 1;
            buf->in_BeginEnd = 1;
            crPackSetBuffer( thread->packer, buf );

            thread->BeginEndState = 0;
        }
    }
}

void PACKSPU_APIENTRY packspu_End( void )
{
    GET_THREAD(thread);
    CRPackBuffer *buf = &thread->BeginEndBuffer;

    if ( thread->netServer.conn->Barf &&
        (thread->BeginEndMode == GL_LINES
        || thread->BeginEndMode == GL_TRIANGLES
        || thread->BeginEndMode == GL_QUADS
        || thread->BeginEndMode == GL_POLYGON ) )
    {
        CRASSERT(buf->pack);

        crPackReleaseBuffer( thread->packer );
        crPackSetBuffer( thread->packer, &thread->normBuffer );
        if ( !crPackCanHoldBuffer( buf ) )
            packspuFlush( (void *) thread );

        crPackAppendBuffer( buf );
        crNetFree( thread->netServer.conn, buf->pack );
        buf->pack = NULL;
    }

    if (pack_spu.swap)
    {
        crPackEndSWAP();
    }
    else
    {
        crPackEnd();
    }
}

static void DoVertex( void )
{
    GET_THREAD(thread);
    CRPackBuffer *buf = &thread->BeginEndBuffer;
    CRPackBuffer *gbuf = &thread->normBuffer;
    int num_data;
    int num_opcode;

    /*crDebug( "really doing Vertex" );*/
    crPackReleaseBuffer( thread->packer );
    num_data = buf->data_current - buf->data_start;
    num_opcode = buf->opcode_start - buf->opcode_current;
    crPackSetBuffer( thread->packer, gbuf );
    if ( !crPackCanHoldBuffer( buf ) )
        /* doesn't hold, first flush gbuf*/
        packspuFlush( (void *) thread );

    crPackAppendBuffer( buf );
    crPackReleaseBuffer( thread->packer );
    crPackSetBuffer( thread->packer, buf );
    crPackResetPointers(thread->packer);
}

static void RunState( void )
{
    GET_THREAD(thread);
    if (! thread->netServer.conn->Barf ) return;
    if (thread->BeginEndState == -1) return;
    switch(thread->BeginEndMode) {
    case GL_POLYGON:
        return;
    case GL_LINES:
        thread->BeginEndState = (thread->BeginEndState + 1) % 2;
        if (thread->BeginEndState)
            return;
        break;
    case GL_TRIANGLES:
        thread->BeginEndState = (thread->BeginEndState + 1) % 3;
        if (thread->BeginEndState)
            return;
        break;
    case GL_QUADS:
        thread->BeginEndState = (thread->BeginEndState + 1) % 4;
        if (thread->BeginEndState)
            return;
        break;
    }
    DoVertex();
}
"""

keys = apiutil.GetDispatchedFunctions(sys.argv[1]+"/APIspec.txt")

for func_name in apiutil.AllSpecials( "packspu_vertex" ):
    params = apiutil.Parameters(func_name)
    print 'void PACKSPU_APIENTRY packspu_%s( %s )' % ( func_name, apiutil.MakeDeclarationString(params) )
    print '{'
    print '\tif (pack_spu.swap)'
    print '\t{'
    print '\t\tcrPack%sSWAP( %s );' % ( func_name, apiutil.MakeCallString( params ) )
    print '\t}'
    print '\telse'
    print '\t{'
    print '\t\tcrPack%s( %s );' % ( func_name, apiutil.MakeCallString( params ) )
    print '\t}'
    print '\tRunState();'
    print '}'
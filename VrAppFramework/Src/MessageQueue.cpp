/************************************************************************************

Filename    :   MessageQueue.cpp
Content     :   Thread communication by string commands
Created     :   October 15, 2013
Authors     :   John Carmack

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include "MessageQueue.h"

#include <stdlib.h>
#include <stdio.h>

#include "Kernel/OVR_LogUtils.h"

namespace OVR
{

bool ovrMessageQueue::debug = false;

ovrMessageQueue::ovrMessageQueue( int maxMessages_ ) :
	shutdown( false ),
	maxMessages( maxMessages_ ),
	messages( new message_t[ maxMessages_ ] ),
	head( 0 ),
	tail( 0 ),
	synced( false )
{
	OVR_ASSERT( maxMessages > 0 );

	for ( int i = 0; i < maxMessages; i++ )
	{
		messages[i].string = NULL;
		messages[i].synced = false;
	}
}

ovrMessageQueue::~ovrMessageQueue()
{
	// Free any messages remaining on the queue.
	for ( ; ; )
	{
		const char * msg = GetNextMessage();
		if ( !msg ) {
			break;
		}
		OVR_LOG( "%p:~ovrMessageQueue: still on queue: %s", this, msg );
		free( (void *)msg );
	}

	// Free the queue itself.
	delete[] messages;
}

void ovrMessageQueue::Shutdown()
{
	OVR_LOG( "%p:ovrMessageQueue shutdown", this );
	shutdown = true;
}

// Thread safe, callable by any thread.
// The msg text is copied off before return, the caller can free
// the buffer.
// The app will abort() with a dump of all messages if the message
// buffer overflows.
bool ovrMessageQueue::PostMessage( const char * msg, bool sync, bool abortIfFull )
{
	if ( shutdown )
	{
		OVR_LOG( "%p:PostMessage( %s ) to shutdown queue", this, msg );
		return false;
	}
	if ( debug )
	{
		OVR_LOG( "%p:PostMessage( %s )", this, msg );
	}

	mutex.DoLock();
	if ( tail - head >= maxMessages )
	{
		mutex.Unlock();
		if ( abortIfFull )
		{
			OVR_LOG( "ovrMessageQueue overflow" );
			for ( int i = head; i < tail; i++ )
			{
				OVR_LOG( "%s", messages[i % maxMessages].string );
			}
			OVR_FAIL( "Message buffer overflowed" );
		}
		return false;
	}
	const int index = tail % maxMessages;
	messages[index].string = OVR_strdup( msg );
	messages[index].synced = sync;
	tail++;
	posted.NotifyAll();
	if ( sync )
	{
		processed.Wait( &mutex );
	}
	mutex.Unlock();

	return true;
}

void ovrMessageQueue::PostString( const char * msg )
{
	PostMessage( msg, false, true );
}

void ovrMessageQueue::PostPrintf( const char * fmt, ... )
{
	char bigBuffer[4096];
	va_list	args;
	va_start( args, fmt );
	vsnprintf( bigBuffer, sizeof( bigBuffer ), fmt, args );
	va_end( args );
	PostMessage( bigBuffer, false, true );
}

bool ovrMessageQueue::PostPrintfIfSpaceAvailable( const int requiredSpace, const char * fmt, ... )
{
	if ( SpaceAvailable() < requiredSpace )
	{
		return false;
	}
	char bigBuffer[4096];
	va_list	args;
	va_start( args, fmt );
	vsnprintf( bigBuffer, sizeof( bigBuffer ), fmt, args );
	va_end( args );
	PostMessage( bigBuffer, false, true );
	return true;
}

bool ovrMessageQueue::TryPostString( const char * msg )
{
	return PostMessage( msg, false, false );
}

bool ovrMessageQueue::TryPostPrintf( const char * fmt, ... )
{
	char bigBuffer[4096];
	va_list	args;
	va_start( args, fmt );
	vsnprintf( bigBuffer, sizeof( bigBuffer ), fmt, args );
	va_end( args );
	return PostMessage( bigBuffer, false, false );
}

void ovrMessageQueue::SendString( const char * msg )
{
	PostMessage( msg, true, true );
}

void ovrMessageQueue::SendPrintf( const char * fmt, ... )
{
	char bigBuffer[4096];
	va_list	args;
	va_start( args, fmt );
	vsnprintf( bigBuffer, sizeof( bigBuffer ), fmt, args );
	va_end( args );
	PostMessage( bigBuffer, true, true );
}

// Returns false if there are no more messages, otherwise returns
// a string that the caller must free.
const char * ovrMessageQueue::GetNextMessage()
{
	NotifyMessageProcessed();

	mutex.DoLock();
	if ( tail <= head )
	{
		mutex.Unlock();
		return NULL;
	}

	const int index = head % maxMessages;
	const char * msg = messages[index].string;
	synced = messages[index].synced;
	messages[index].string = NULL;
	messages[index].synced = false;
	head++;
	mutex.Unlock();

	if ( debug )
	{
		OVR_LOG( "%p:GetNextMessage() : %s", this, msg );
	}

	return msg;
}

// Returns immediately if there is already a message in the queue.
void ovrMessageQueue::SleepUntilMessage()
{
	NotifyMessageProcessed();

	mutex.DoLock();
	if ( tail > head )
	{
		mutex.Unlock();
		return;
	}

	if ( debug )
	{
		OVR_LOG( "%p:SleepUntilMessage() : sleep", this );
	}

	posted.Wait( & mutex );
	mutex.Unlock();

	if ( debug )
	{
		OVR_LOG( "%p:SleepUntilMessage() : awoke", this );
	}
}

void ovrMessageQueue::NotifyMessageProcessed()
{
	if ( synced )
	{
		processed.NotifyAll();
		synced = false;
	}
}

void ovrMessageQueue::ClearMessages()
{
	if ( debug )
	{
		OVR_LOG( "%p:ClearMessages()", this );
	}
	for ( const char * msg = GetNextMessage(); msg != NULL; msg = GetNextMessage() )
	{
		OVR_LOG( "%p:ClearMessages: discarding %s", this, msg );
		free( (void *)msg );
	}
}

}	// namespace OVR

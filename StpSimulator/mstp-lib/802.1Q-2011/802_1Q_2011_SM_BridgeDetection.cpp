
// This file is part of the mstp-lib library, available at http://sourceforge.net/projects/mstp-lib/ 
// Copyright (c) 2011-2014 Adrian Gostin, distributed under the GNU General Public License v3.

#include "802_1Q_2011_procedures.h"
#include "../bridge.h"
#include "../port.h"
#include "../log.h"
#include "assert.h"

// See �13.31 in 802.1Q-2011

enum
{
	UNDEFINED,
	NOT_EDGE,
	EDGE,
	ISOLATED,
};

// ============================================================================

const char* BridgeDetection_802_1Q_2011_GetStateName (SM_STATE state)
{
	switch (state)
	{
		case NOT_EDGE:	return "NOT_EDGE";
		case EDGE:		return "EDGE";
		case ISOLATED:	return "ISOLATED";
		default:		return "(undefined)";
	}
}

// ============================================================================

SM_STATE BridgeDetection_802_1Q_2011_CheckConditions (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];
	
	// ------------------------------------------------------------------------
	// Check global conditions.
	
	if (bridge->BEGIN && !port->AdminEdge)
	{
		if (state == NOT_EDGE)
		{
			// The entry block for this state has been executed already.
			return 0;
		}
		
		return NOT_EDGE;
	}

	if (bridge->BEGIN && port->AdminEdge)
	{
		if (state == EDGE)
		{
			// The entry block for this state has been executed already.
			return 0;
		}
		
		return EDGE;
	}
	
	// ------------------------------------------------------------------------
	// Check exit conditions from each state.
	
	if (state == NOT_EDGE)
	{
		// Missing info in the specs: "proposing" of WHICH tree??
		// This is clarified in this document (Chapter 4, paragraph b): http://www.ieee802.org/1/files/public/docs2008/aq-seaman-fragile-bridges-0908.pdf
		//
		// We changed this condition slightly because we were looping endlessly between EDGE and NOT_EDGE.
		// The condition specified in 802.1Q-2011 was:
		//
		//	if ((!port->portEnabled && port->AdminEdge) ||
		//		((port->edgeDelayWhile == 0) && port->AutoEdge && port->sendRSTP && port->trees [CIST_INDEX]->proposing))

		if ((!port->portEnabled && port->AdminEdge) ||
			(port->portEnabled && (port->edgeDelayWhile == 0) && port->AutoEdge && port->sendRSTP && port->trees [CIST_INDEX]->proposing))
		{
			return EDGE;
		}

		if ((port->edgeDelayWhile == 0) && !port->AdminEdge && !port->AutoEdge && port->sendRSTP && port->trees [CIST_INDEX]->proposing && port->operPointToPointMAC)
		{
			return ISOLATED;
		}

		return 0;
	}

	if (state == EDGE)
	{
		// We changed this condition slightly because we were looping endlessly between EDGE and NOT_EDGE.
		// The condition specified in 802.1Q-2011 was:
		//
		// if (((!port->portEnabled || !port->AutoEdge) && !port->AdminEdge) || !port->operEdge)
		
		if (((!port->portEnabled || !port->AutoEdge) && !port->AdminEdge) || (port->portEnabled && !port->operEdge))
		{
			return NOT_EDGE;
		}

		return 0;
	}
	
	if (state == ISOLATED)
	{
		if (port->AdminEdge || port->AutoEdge || !port->isolate || !port->operPointToPointMAC)
		{
			return NOT_EDGE;
		}

		return 0;
	}

	assert (false);
	return 0;
}

// ============================================================================

void BridgeDetection_802_1Q_2011_InitState (STP_BRIDGE* bridge, int givenPort, int givenTree, SM_STATE state, unsigned int timestamp)
{
	assert (givenPort != -1);
	assert (givenTree == -1);

	PORT* port = bridge->ports [givenPort];
	
	if (state == EDGE)
	{
		port->operEdge = true;
	}
	else if (state == NOT_EDGE)
	{
		port->operEdge = false;
	}
	else
		assert (false);
}


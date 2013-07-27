#include "OnBoardManager.h"

#include "../common/Ninja.h"
#include "../common/Heartbeat.h"

#include "NinjaLED.h"

#include "../decoder/CommonProtocolDecoder.h"
#include "../encoder/CommonProtocolEncoder.h"

#include "../decoder/WT450ProtocolDecoder.h"

#include "../decoder/arlecProtocolDecoder.h"
#include "../encoder/arlecProtocolEncoder.h"

#include "../decoder/HE330v2ProtocolDecoder.h"
#include "../encoder/HE330v2ProtocolEncoder.h"

#include "../decoder/OSv2ProtocolDecoder.h"
#include "../encoder/OSv2ProtocolEncoder.h"

#include "../decoder/bInDProtocolDecoder.h"
#include "../encoder/bInDProtocolEncoder.h"

extern NinjaLED leds;

OnBoardManager::OnBoardManager()
{
	m_nLastDecode = -1;

	m_Decoders[0] = new CommonProtocolDecoder();
	m_Decoders[1] = new WT450ProtocolDecoder();
	m_Decoders[2] = new arlecProtocolDecoder();
	m_Decoders[3] = new HE330v2ProtocolDecoder();
	m_Decoders[4] = new OSv2ProtocolDecoder();
	m_Decoders[5] = new bInDProtocolDecoder();
	
}

void OnBoardManager::setup()
{
	m_Transmitter.setup(TRANSMIT_PIN);

	m_Receiver.start();
}

void OnBoardManager::check()
{
	RFPacket*	pReceivedPacket = m_Receiver.getPacket();

	// Check for unhandled RF data first
	if(pReceivedPacket != NULL)
	{
		bool bDecodeSuccessful = false;
		m_nLastDecode = -1;
		//if (pReceivedPacket->getSize() == 50)
			//	pReceivedPacket->print();			//debug

		for(int i = 0; i < NUM_DECODERS; i++)
		{
			if(m_Decoders[i]->decode(pReceivedPacket))
			{
				m_nLastDecode = i;

				bDecodeSuccessful = true;
			}
			pReceivedPacket->rewind();
		}

		if(bDecodeSuccessful)
		{
			// Blink stat LED to show activity
			leds.blinkStat();

			NinjaPacket packet;
			
			m_Decoders[m_nLastDecode]->fillPacket(&packet);
			
			packet.printToSerial();
		}

		// Purge 
		m_Receiver.purge();
	}

	// Check if heartbeat expired
	if(heartbeat.isExpired())
	{
		NinjaPacket packet;

		packet.setType(TYPE_DEVICE);
		packet.setGuid(0);
		packet.setDevice(ID_STATUS_LED);
		packet.setData(leds.getStatColor());

		packet.printToSerial();
		
		packet.setDevice(ID_NINJA_EYES);
		packet.setData(leds.getEyesColor());

		packet.printToSerial();
	}
}

void OnBoardManager::handle(NinjaPacket* pPacket)
{
	if(pPacket->getGuid() != 0)
		return;

	if(pPacket->getDevice() == ID_STATUS_LED)
		leds.setStatColor(pPacket->getData());
	else if(pPacket->getDevice() == ID_NINJA_EYES)
		leds.setEyesColor(pPacket->getData());
	else if(pPacket->getDevice() == ID_ONBOARD_RF)
	{
		m_Receiver.stop();
		
		char encoding = pPacket->getEncoding();
		
		switch (encoding)
		{
			case ENCODING_COMMON:
				m_encoder = new CommonProtocolEncoder(pPacket->getTiming());
				break;
			case ENCODING_ARLEC:
				m_encoder = new arlecProtocolEncoder(pPacket->getTiming());
				break;
			case ENCODING_HE330:
				m_encoder = new HE330v2ProtocolEncoder(pPacket->getTiming());
				break;
			case ENCODING_OSV2:
				m_encoder = new OSv2ProtocolEncoder(pPacket->getTiming());
				break;
			case ENCODING_BIND:
				m_encoder = new bInDProtocolEncoder(pPacket->getTiming());
				break;				
		}
		
		if(pPacket->isDataInArray())
			m_encoder->setCode(pPacket->getDataArray(), pPacket->getArraySize());
		else 
			m_encoder->setCode(pPacket->getData());
			
		//m_encoder->setCode(pPacket->getData());
		m_encoder->encode(&m_PacketTransmit);
		
		m_Transmitter.send(&m_PacketTransmit, 5);
		delete m_encoder;
		m_Receiver.start();
	}

	pPacket->setType(TYPE_ACK);
	pPacket->printToSerial();
}
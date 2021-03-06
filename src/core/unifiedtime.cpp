/*******************************************************************************************
 
			Hash(BEGIN(Satoshi[2010]), END(Sunny[2012])) == Videlicet[2014] ++
   
 [Learn and Create] Viz. http://www.opensource.org/licenses/mit-license.php
  
*******************************************************************************************/

#include "unifiedtime.h"
#include "../LLP/client.h"
#include <inttypes.h>


using namespace std;


/** Flag Declarations **/
bool fTimeUnified = false;
bool fIsTimeSeed = false;
bool fIsSeedNode = false;


int UNIFIED_AVERAGE_OFFSET = 0;
int UNIFIED_MOVING_ITERATOR = 0;


/** Unified Time Declarations **/
vector<int> UNIFIED_TIME_DATA;
vector<Net::CAddress> SEED_NODES;

/** Declarations for the DNS Seed Nodes. **/
const char* DNS_SeedNodes[] = 
{
	"node1.nexusoft.io",
	"node2.nexusoft.io",
	"node3.nexusoft.io",
	"node4.nexusoft.io",
	"node5.nexusoft.io",
	"node6.nexusoft.io",
	"node7.nexusoft.io",
	"node8.nexusoft.io",
	"node9.nexusoft.io",
	"node10.nexusoft.io",
	"node11.nexusoft.io",
	"node12.nexusoft.io",
	"node13.nexusoft.io",
	"node1.nexusearth.com",
	"node2.nexusearth.com",
	"node3.nexusearth.com",
	"node4.nexusearth.com",
	"node5.nexusearth.com",
	"node6.nexusearth.com",
	"node7.nexusearth.com",
	"node8.nexusearth.com",
	"node9.nexusearth.com",
	"node10.nexusearth.com",
	"node11.nexusearth.com",
	"node12.nexusearth.com",
	"node13.nexusearth.com",
	"node1.mercuryminer.com",
	"node2.mercuryminer.com",
	"node3.mercuryminer.com",
	"node4.mercuryminer.com",
	"node5.mercuryminer.com",
	"node6.mercuryminer.com",
	"node7.mercuryminer.com",
	"node8.mercuryminer.com",
	"node9.mercuryminer.com",
	"node10.mercuryminer.com",
	"node11.mercuryminer.com",
	"node12.mercuryminer.com",
	"node13.mercuryminer.com",
	"node1.nexusminingpool.com",
	"node2.nexusminingpool.com"
};

/** Declarations for the DNS Seed Nodes. **/
const char* DNS_SeedNodes_Testnet[] = 
{
	"test1.nexusoft.io"
};

/** Seed Nodes for Unified Time. **/
vector<string> SEEDS;

/** Baseline Maximum Values for Unified Time. **/
int MAX_UNIFIED_DRIFT   = 20;
int MAX_UNIFIED_SAMPLES = 20;


/** Gets the UNIX Timestamp from your Local Clock **/
int64 GetLocalTimestamp(){ return time(NULL); }


/** Gets the UNIX Timestamp from the Nexus Network **/
int64 GetUnifiedTimestamp(){ return GetLocalTimestamp() + UNIFIED_AVERAGE_OFFSET; }


/** Called from Thread Time Regulator.
    This keeps time keeping separate from regular processing. **/
void InitializeUnifiedTime()
{
	Wallet::CTimeDB init("cr"); ///init("cw")
	fTimeUnified = init.ReadTimeData(UNIFIED_AVERAGE_OFFSET);
    init.Close();
	
	printf("***** Unified Time Initialized to %i\n", UNIFIED_AVERAGE_OFFSET);
}


/** Calculate the Average Unified Time. Called after Time Data is Added **/
int GetUnifiedAverage()
{
	if(UNIFIED_TIME_DATA.empty())
		return UNIFIED_AVERAGE_OFFSET;
		
	int nAverage = 0;
	for(int index = 0; index < std::min(MAX_UNIFIED_SAMPLES, (int)UNIFIED_TIME_DATA.size()); index++)
		nAverage += UNIFIED_TIME_DATA[index];
		
	return round(nAverage / (double) std::min(MAX_UNIFIED_SAMPLES, (int)UNIFIED_TIME_DATA.size()));
}


/** Regulator of the Unified Clock **/
void ThreadTimeRegulator(void* parg)
{
	/** Compile the Seed Nodes into a set of Vectors. **/
	SEED_NODES    = DNS_Lookup(fTestNet ? DNS_SeedNodes_Testnet : DNS_SeedNodes);
	
	/** Iterator to be used to ensure every time seed is giving an equal weight towards the Global Seeds. **/
	int nIterator = 0;
	
	/** Compile all the Seeds into one Vector. **/
	printf("Compiling List...\n");
	
	for(int nIndex = 0; nIndex < SEED_NODES.size(); nIndex++)
		SEEDS.push_back(SEED_NODES[nIndex].ToStringIP());
	
	/** The Entry Client Loop for Core LLP. **/
	string ADDRESS = "";
	LLP::CoreOutbound SERVER("", strprintf("%u", (fTestNet ? TESTNET_CORE_LLP_PORT : NEXUS_CORE_LLP_PORT)));
	loop
	{
		try
		{
		
			/** Increment the Time Seed Connection Iterator. **/
			nIterator++;
			
			
			/** Reset the ITerator if out of Seeds. **/
			if(nIterator == SEEDS.size())
			   nIterator = 0;
				
				
			/** Connect to the Next Seed in the Iterator. **/
			SERVER.IP = SEEDS[nIterator];
			SERVER.Connect();
			
			
			/** If the Core LLP isn't connected, Retry in 10 Seconds. **/
			if(!SERVER.Connected())
			{
				printf("***** Core LLP: Failed To Connect To %s:%s\n", SERVER.IP.c_str(), SERVER.PORT.c_str());
				
				continue;
			}

			
			/** Use a CMajority to Find the Sample with the Most Weight. **/
			CMajority<int> nSamples;
			
			
			/** Get 10 Samples From Server. **/
			SERVER.GetOffset((unsigned int)GetLocalTimestamp());
				
				
			/** Read the Samples from the Server. **/
			while(SERVER.Connected() && !SERVER.Errors() && !SERVER.Timeout(5))
			{
				Sleep(1);
			
				SERVER.ReadPacket();
				if(SERVER.PacketComplete())
				{
					LLP::Packet PACKET = SERVER.NewPacket();
					
					/** Add a New Sample each Time Packet Arrives. **/
					if(PACKET.HEADER == SERVER.TIME_OFFSET)
					{
						int nOffset = bytes2int(PACKET.DATA);
						nSamples.Add(nOffset);
						
						SERVER.GetOffset((unsigned int)GetLocalTimestamp());
						printf("***** Core LLP: Added Sample %i | Seed %s\n", nOffset, SERVER.IP.c_str());
					}
					
					SERVER.ResetPacket();
				}
				
				/** Close the Connection Gracefully if Received all Packets. **/
				if(nSamples.Samples() == 9)
				{
					SERVER.Close();
					break;
				}
			}
			
			
			/** If there are no Successful Samples, Try another Connection. **/
			if(nSamples.Samples() == 0)
			{
				printf("***** Core LLP: Failed To Get Time Samples.\n");
				SERVER.Close();
				
				continue;
			}
			
			
			/** If the Moving Average is filled with samples, continue iterating to keep it moving. **/
			if(UNIFIED_TIME_DATA.size() >= MAX_UNIFIED_SAMPLES)
			{
				if(UNIFIED_MOVING_ITERATOR >= MAX_UNIFIED_SAMPLES)
					UNIFIED_MOVING_ITERATOR = 0;
								
				UNIFIED_TIME_DATA[UNIFIED_MOVING_ITERATOR] = nSamples.Majority();
				UNIFIED_MOVING_ITERATOR ++;
			}
			
			
			/** If The Moving Average is filling, move the iterator along with the Time Data Size. **/
			else
			{
				UNIFIED_MOVING_ITERATOR = UNIFIED_TIME_DATA.size();
				UNIFIED_TIME_DATA.push_back(nSamples.Majority());
			}
			

			/** Update Iterators and Flags. **/
			if((UNIFIED_TIME_DATA.size() > 0))
			{
				fTimeUnified = true;
				UNIFIED_AVERAGE_OFFSET = GetUnifiedAverage();
				
				Wallet::CTimeDB DB("cw");
				DB.WriteTimeData(UNIFIED_AVERAGE_OFFSET);
				DB.Close();
				
				printf("***** %i Iterator | %i Offset | %i Current | %"PRId64"\n", UNIFIED_MOVING_ITERATOR, nSamples.Majority(), UNIFIED_AVERAGE_OFFSET, GetUnifiedTimestamp());
				
				
				/** Regulate the Clock while Waiting, and Break if the Clock Changes. **/
				unsigned int nSeconds = 0;
				while(nSeconds < 600)
				{
					int64 nTimestamp = GetLocalTimestamp();
					Sleep(1000);
					
					int64 nElapsed = GetLocalTimestamp() - nTimestamp;
					if(nElapsed > (MAX_UNIFIED_DRIFT + 1) || nElapsed < ((MAX_UNIFIED_DRIFT - 1) * -1))
					{
						UNIFIED_TIME_DATA.clear();
						UNIFIED_AVERAGE_OFFSET -= nElapsed;
						
						printf("***** LLP Clock Regulator: Time Changed by %"PRId64" Seconds. New Offset %i\n", nElapsed, UNIFIED_AVERAGE_OFFSET);
						
						break;
					}
					
					nSeconds++;
				}
			}
		}
		catch(std::exception& e){ printf("UTM ERROR: %s\n", e.what()); }
	}
}

/** DNS Query of Domain Names Associated with Seed Nodes **/
vector<Net::CAddress> DNS_Lookup(const char* DNS_Seed[])
{
	vector<Net::CAddress> vNodes;
    for (unsigned int seed = 0; seed < (fTestNet ? 1 : 26); seed++)
	{
		printf("%u Host: %s\n", seed, DNS_Seed[seed]);
        vector<Net::CNetAddr> vaddr;
        if (Net::LookupHost(DNS_Seed[seed], vaddr))
        {
            BOOST_FOREACH(Net::CNetAddr& ip, vaddr)
            {
                Net::CAddress addr = Net::CAddress(Net::CService(ip, Net::GetDefaultPort()));
                vNodes.push_back(addr);
				
				printf("DNS Seed: %s\n", addr.ToStringIP().c_str());
				
				Net::addrman.Add(addr, ip, true);
            }
        }
    }
	
	return vNodes;
}


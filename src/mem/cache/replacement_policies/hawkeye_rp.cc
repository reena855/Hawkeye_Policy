/**
 * Copyright (c) 2018 Inria
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
 */

#include "mem/cache/replacement_policies/hawkeye_rp.hh"

#include <cassert>
#include <memory>
#include <vector>

#include "debug/CacheRepl.hh"
#include "params/HAWKEYERP.hh"
#include "sim/stats.hh"

// L3 data
// Size: 2MB=2*1024*1024B, assoc: 16, Block-size: 64B
// #sets = (2*1024*1024)/(16*64) = 2048 -> 11 bits
// #block-offset bits = 6 bits
// #tag bits = 15 bits
// addr = [tag: 15].[index: 11].[boffset: 6]

// L3 specific derived-params

uint32_t numSets=2048;
const unsigned setMask = numSets-1;
const int setShift = 6; // Block Offset
const int tagShift = 17; 
const int PCMapSize = 8*1024;

unsigned head_ptr[2048];
Addr AccessHistory[2048][128]; // 128 = 16*8
unsigned OccupancyVector[2048][128];
unsigned HawkeyePredictor[8*1024];


//static uint64_t
//CRC(uint64_t addr) {
//
//    unsigned long long crcPolynomial = 3988292384ULL;  //Decimal value for 0xEDB88320 hex value
//    unsigned long long result = addr;
//    for(unsigned int i = 0; i < 32; i++ ) {
//    	if((result & 1 ) == 1 ){
//    		result = (result >> 1) ^ crcPolynomial;
//    	}
//    	else{
//    		result >>= 1;
//    	}
//    }
//
//    return result;
//}

unsigned
OPTgen(uint64_t addr) {

// extract tag and set 

    Addr tag = addr >> tagShift; 

    uint32_t set = (addr >> setShift) & setMask;

    fatal_if(set>2047, "# of sets must be less than 2048");
    fatal_if(tag>32767, "# of tags must be less than 2^15");

    DPRINTF(CacheRepl, "HAWKEYE RP: Update Predictor: Addr:  %s," 
    " Tag: %s, Set: %s\n", addr, tag, set);
// OPTGen
// AccessHistory[set][entries]
// OccupancyVector[set][entries]
  
    AccessHistory[set][head_ptr[set]] = tag;
    OccupancyVector[set][head_ptr[set]] = 1;
    
    // check if this is the first access
    unsigned match_index = 0;

    bool repeatAccess = false;
    bool OPT_decision = false;
    bool full = false;

    // Check the recent history
    for (int i=head_ptr[set]-1; i>=0; i--){
	if (AccessHistory[set][i] == tag){
             repeatAccess = true;
             match_index = i;
             break;
        }
    }
    
    // if previous access exists, update occupancy vector
    if (repeatAccess) {
        for (int i = match_index; i < head_ptr[set]; i++) {
            if (OccupancyVector[set][i] == 16) {
                full = true;
            }
        }

        if (!full) {
            OPT_decision = true;
            for (int i = match_index; i < head_ptr[set]; i++) {
	        OccupancyVector[set][i]++;
            }
        }
    }    

    // if no match, search older history
    else {
        for (int i=127; i>head_ptr[set]; i--){
	    if (AccessHistory[set][i] == tag){
                 repeatAccess = true;
                 match_index = i;
                 break;
            }
        }
        if (repeatAccess) {
            for (int i = match_index; i < 128; i++) {
                if (OccupancyVector[set][i] == 16) {
                    full = true;
                }
            }
            
            for (int i = 0; i < head_ptr[set]; i++) {
                if (OccupancyVector[set][i] == 16) {
                    full = true;
                }
            }

            if (!full) {
                OPT_decision = true;
                for (int i = match_index; i < 128; i++) {
                    OccupancyVector[set][i]++;
                }
                for (int i = 0; i < head_ptr[set]; i++) {
                    OccupancyVector[set][i]++;
                }
            }
        }    
    }
    
    head_ptr[set]++;
    if (head_ptr[set] == 128) head_ptr[set] = 0; 
    
    unsigned OPT_hit;
    
    if (repeatAccess && OPT_decision) { // OPT hit 
        OPT_hit = 1; 
    }

    else if (repeatAccess && !OPT_decision){ // OPT miss
        OPT_hit = 0;
    }
 
    else {
        OPT_hit = 2;
    }
    
    DPRINTF(CacheRepl, "HAWKEYE OPTgen: prediction: %s," 
    " Tag: %s, Set: %s\n", OPT_hit, tag, set);

    return OPT_hit;

}






HAWKEYERP::HAWKEYERP(const Params *p)
    : BaseReplacementPolicy(p)
{
   
}

void
HAWKEYERP::invalidate(const std::shared_ptr<ReplacementData>& replacement_data)
const
{
    // Reset last touch timestamp
    std::shared_ptr<HAWKEYEReplData> casted_replacement_data =
        std::static_pointer_cast<HAWKEYEReplData>(replacement_data);
    
    casted_replacement_data->rrip = 7;
}

void
HAWKEYERP::touch(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    std::shared_ptr<HAWKEYEReplData> casted_replacement_data =
        std::static_pointer_cast<HAWKEYEReplData>(replacement_data);

    // setting RRIP
    
    if (casted_replacement_data->cacheFriendly) {
        casted_replacement_data->rrip = 0;
    }
    
    else {
        casted_replacement_data->rrip = 7;
    }

    DPRINTF(CacheRepl, "HAWKEYE TOUCH: Setting RRIP: %s\n",
		casted_replacement_data->rrip);

}


unsigned 
HAWKEYERP::update_predictor(Addr addr) const
{


// OPTgen

    unsigned OPT_hit;

    OPT_hit = OPTgen(addr);

// Hawkeye predictor

    Addr hashedPC = addr % PCMapSize;
    fatal_if(hashedPC>8191, "# of sets must be less than 8*1024");

    if (OPT_hit == 1) { // OPT hit
        if (HawkeyePredictor[hashedPC] < 7) {
            HawkeyePredictor[hashedPC]++; // only 3 bits
        }
    }

    else if (OPT_hit == 0) { // OPT miss
        if (HawkeyePredictor[hashedPC] != 0) {
            HawkeyePredictor[hashedPC]--; // never below 0
        }
    }
    
    DPRINTF(CacheRepl, "HAWKEYE Update Predictor: Hawkeye PC Map Val: %s," 
    " at hashedPC: %s\n", HawkeyePredictor[hashedPC], hashedPC);

    return OPT_hit;
};


bool 
HAWKEYERP::predict(const std::shared_ptr<ReplacementData>& replacement_data, Addr addr) const
{

// extract hashedPC

    Addr hashedPC = addr % PCMapSize;
    fatal_if(hashedPC>8191, "# of sets must be less than 8*1024");


    std::shared_ptr<HAWKEYEReplData> casted_replacement_data =
        std::static_pointer_cast<HAWKEYEReplData>(replacement_data);

    // Predictor decision and setting RRIP
    
    casted_replacement_data->hashedPC = hashedPC;

    if (HawkeyePredictor[hashedPC] > 3) {
        casted_replacement_data->cacheFriendly = true;
    }
    
    else {
        casted_replacement_data->cacheFriendly = false;
    }

    DPRINTF(CacheRepl, "HAWKEYE predict: Hawkeye cacheFriendly prediction: %s"
              " for hashedPC %s\n", casted_replacement_data->cacheFriendly, 
                  casted_replacement_data->hashedPC);

    return casted_replacement_data->cacheFriendly;

};

void
HAWKEYERP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    std::shared_ptr<HAWKEYEReplData> casted_replacement_data =
        std::static_pointer_cast<HAWKEYEReplData>(replacement_data);


    if (casted_replacement_data->cacheFriendly) {
        casted_replacement_data->rrip = 0;
    }

    else {
        casted_replacement_data->rrip = 7;
    }


    DPRINTF(CacheRepl, "HAWKEYE RESET: Inserting Victim, cacheFriendly prediction: %s"
		" RRIP: %s\n", casted_replacement_data->cacheFriendly, 
		casted_replacement_data->rrip);

}

ReplaceableEntry*
HAWKEYERP::getVictim(const ReplacementCandidates& candidates) const
{


    // There must be at least one replacement candidate
    assert(candidates.size() > 0);

    // Visit all candidates to find victim
    ReplaceableEntry* victim = candidates[0];
    for (const auto& candidate : candidates) {
        // Update victim entry if necessary
        if (std::static_pointer_cast<HAWKEYEReplData>(
                    candidate->replacementData)->rrip >
                std::static_pointer_cast<HAWKEYEReplData>(
                    victim->replacementData)->rrip) {
            victim = candidate;
        }
    }
    return victim;
}

void
HAWKEYERP::age(const ReplacementCandidates& candidates) const
{                    
     // There must be at least one replacement candidate
     assert(candidates.size() > 0);
     for (const auto& candidate : candidates) {
         
         if (std::static_pointer_cast<HAWKEYEReplData>(
                  candidate->replacementData)->cacheFriendly) {

         	if (std::static_pointer_cast<HAWKEYEReplData>(
                          candidate->replacementData)->rrip < 6) {
         		
         		std::static_pointer_cast<HAWKEYEReplData>(
                            candidate->replacementData)->rrip++;
         	}
         }
     }
}

   	
bool
HAWKEYERP::victim_check(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    std::shared_ptr<HAWKEYEReplData> casted_replacement_data =
        std::static_pointer_cast<HAWKEYEReplData>(replacement_data);

    if (casted_replacement_data->cacheFriendly) { // cache friendly block evicted
    

        DPRINTF(CacheRepl, "HAWKEYE getVictim: Victim predicted cacheFriendly,"
		" updating HP entry at hashed PC %s", 
		        casted_replacement_data->hashedPC);


        if (HawkeyePredictor[casted_replacement_data->hashedPC] != 0) {
            HawkeyePredictor[casted_replacement_data->hashedPC]--;
        }
     }

    return casted_replacement_data->cacheFriendly;
}

std::shared_ptr<ReplacementData>
HAWKEYERP::instantiateEntry()
{
    for (int i=0; i<2048; i++) {
        head_ptr[i] = 0;
        for (int j=0; j<8*16; j++) {
        	AccessHistory[i][j] = -1;
        	OccupancyVector[i][j] = 0;
        }
    }

    for (int i=0; i<8*1024; i++) {
        HawkeyePredictor[i] = 4;
    }

    return std::shared_ptr<ReplacementData>(new HAWKEYEReplData());
}




HAWKEYERP*
HAWKEYERPParams::create()
{
    return new HAWKEYERP(this);
}

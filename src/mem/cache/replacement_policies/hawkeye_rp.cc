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

unsigned head_ptr[2048];
Addr AccessHistory[2048][128]; // 128 = 16*8
unsigned OccupancyVector[2048][128];
unsigned HawkeyePredictor[8*1024];

// L3 specific derived-params



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

    DPRINTF(CacheRepl, "HAWKEYE RP: Setting RRIP: %s\n",
		casted_replacement_data->rrip);

}


void 
HAWKEYERP::update_predictor(Addr addr) const
{

// extract tag and set 

    Addr tag = addr >> tagShift; 

    uint32_t set = (addr >> setShift) & setMask;

    fatal_if(set>2047, "# of sets must be less than 2048");
    fatal_if(tag>32767, "# of tags must be less than 2^15");

// Casting


//    std::shared_ptr<HAWKEYEReplData> casted_replacement_data =
//        std::static_pointer_cast<HAWKEYEReplData>(replacement_data);

// OPTGen
// AccessHistory[set][way]
// OccupancyVector[set][way]
  
  
    AccessHistory[set][head_ptr[set]] = tag;
    OccupancyVector[set][head_ptr[set]] = 0;
    
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
        }
    }
    
    // if previous access exists, update occupancy vector
    if (repeatAccess) {
        for (int i = match_index+1; i < head_ptr[set]; i++) {
            if (OccupancyVector[set][i] == 15) {
                full = true;
            }
        }

        if (!full) {
            OPT_decision = true;
            for (int i = match_index+1; i < head_ptr[set]; i++) {
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
            }
        }
        if (repeatAccess) {
            for (int i = match_index+1; i < 128; i++) {
                if (OccupancyVector[set][i] == 15) {
                    full = true;
                }
            }
            
            for (int i = 0; i < head_ptr[set]; i++) {
                if (OccupancyVector[set][i] == 15) {
                    full = true;
                }
            }

            if (!full) {
                OPT_decision = true;
                for (int i = match_index+1; i < 128; i++) {
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
    
    DPRINTF(CacheRepl, "HAWKEYE RP: OPTgen hit prediction: %s\n", OPT_decision);
 

// Hawkeye predictor
// num entries: 8*1024
// indexed by 13-bit hashed PC

    const int hashedPCShift = 19;
    Addr hashedPC = addr >> hashedPCShift;

    if (repeatAccess && OPT_decision) { // OPT hit 
        if (HawkeyePredictor[hashedPC] < 7) {
            HawkeyePredictor[hashedPC]++; // only 3 bits
        }
    }

    else if (repeatAccess && !OPT_decision) { // OPT miss
        if (HawkeyePredictor[hashedPC] != 0) {
            HawkeyePredictor[hashedPC]--; // never below 0
        }
    }
};


void 
HAWKEYERP::predict(const std::shared_ptr<ReplacementData>& replacement_data, Addr addr) const
{

// extract hashedPC 

    const int hashedPCShift = 19;
    Addr hashedPC = addr >> hashedPCShift;
    fatal_if(hashedPC>8191, "# of sets must be less than 8*1024");


// Casting


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

    DPRINTF(CacheRepl, "HAWKEYE RP: Hawkeye cacheFriendly prediction: %s"
              "for hasedPC %s\n", casted_replacement_data->cacheFriendly, 
                  casted_replacement_data->hashedPC);

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


    DPRINTF(CacheRepl, "HAWKEYE RP: Inserting Victim, cacheFriendly prediction: %s"
		"RRIP: %s\n", casted_replacement_data->cacheFriendly, 
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


                    

    if (std::static_pointer_cast<HAWKEYEReplData>(
             victim->replacementData)->cacheFriendly) { // cache friendly block evicted
    

        DPRINTF(CacheRepl, "HAWKEYE RP: Victim %s was predicted cacheFriendly,"
		"updating HP entry at hashed PC %s", victim, 
		HawkeyePredictor[std::static_pointer_cast<HAWKEYEReplData>(
                  victim->replacementData)->hashedPC]);

        if (HawkeyePredictor[std::static_pointer_cast<HAWKEYEReplData>(
                  victim->replacementData)->hashedPC] != 0) {
            HawkeyePredictor[std::static_pointer_cast<HAWKEYEReplData>(
                  victim->replacementData)->hashedPC]--;
        }
    }

    // Age all the cache_friendly lines
    // The new line will be inserted later
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


    return victim;
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
        HawkeyePredictor[i] = 0;
    }

    return std::shared_ptr<ReplacementData>(new HAWKEYEReplData());
}

HAWKEYERP*
HAWKEYERPParams::create()
{
    return new HAWKEYERP(this);
}

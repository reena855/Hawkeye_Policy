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

#include "params/HAWKEYERP.hh"

unsigned head_ptr[2048];
Addr AccessHistory[2048][128]; // 128 = 16*8
unsigned OccupancyVector[2048][128];
unsigned HawkeyePredictor[8*1024];

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
  // Do Nothing
}


void 
HAWKEYERP::update_state(const std::shared_ptr<ReplacementData>& replacement_data, Addr addr, Addr tag, 
                uint32_t set) const
{

// Casting


    std::shared_ptr<HAWKEYEReplData> casted_replacement_data =
        std::static_pointer_cast<HAWKEYEReplData>(replacement_data);

// OPTGen
// AccessHistory[set][way]
// OccupancyVector[set][way]
    
    AccessHistory[set][head_ptr[set]] = tag;
    head_ptr[set]++;
    if (head_ptr[set] == 128) head_ptr[set] = 0; 
    
    // check if this is the first access
    unsigned match_index = 0;

    bool repeatAccess = false;
    bool OPT_decision = false;


    // Check the recent history
    for (int i=head_ptr[set]; i>=0; i--){
	if (AccessHistory[set][i] == tag){
             repeatAccess = true;
             match_index = i;
        }
    }
    
    // if previous access exists, update occupancy vector
    if (repeatAccess) {
        if (OccupancyVector[set][match_index] < 128) {
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
            }
        }
        if (repeatAccess) {
            if (OccupancyVector[set][match_index] < 128) {
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
 

// Hawkeye predictor
// num entries: 8*1024
// indexed by 13-bit hashed PC

    const int hashedPCShift = 19;
    casted_replacement_data->hashedPC = addr >> hashedPCShift;

    if (repeatAccess && OPT_decision) { // OPT hit 
        if (HawkeyePredictor[casted_replacement_data->hashedPC] < 7) {
            HawkeyePredictor[casted_replacement_data->hashedPC]++; // only 3 bits
        }
    }

    else if (repeatAccess && !OPT_decision) { // OPT miss
        if (HawkeyePredictor[casted_replacement_data->hashedPC] != 0) {
            HawkeyePredictor[casted_replacement_data->hashedPC]--; // never below 0
        }
    }

    // Predictor decision

    if (HawkeyePredictor[casted_replacement_data->hashedPC] > 3) {
        casted_replacement_data->cacheFriendly = true;
    }
    
    else {
        casted_replacement_data->cacheFriendly = false;
    }


// Cache Replacement

    // setting rrip is the same for both
    // hit and miss
    if (casted_replacement_data->cacheFriendly) {
        casted_replacement_data->rrip = 0;
    }
    
    else {
        casted_replacement_data->rrip = 7;
    }

};

void
HAWKEYERP::reset(const std::shared_ptr<ReplacementData>& replacement_data) const
{
    // Set last touch timestamp
    std::shared_ptr<HAWKEYEReplData> casted_replacement_data =
        std::static_pointer_cast<HAWKEYEReplData>(replacement_data);


    if (casted_replacement_data->cacheFriendly) {
        casted_replacement_data->rrip = 0;
    }

    else {
        casted_replacement_data->rrip = 7;
    }

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
        	AccessHistory[i][j] = 0;
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

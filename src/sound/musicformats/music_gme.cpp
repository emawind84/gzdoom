/*
** music_gme.cpp
** General game music player, using Game Music Emu for decoding.
**
**---------------------------------------------------------------------------
** Copyright 2009 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

// HEADER FILES ------------------------------------------------------------

// Uncomment if you are using the DLL version of GME.
//#define GME_DLL

#include <algorithm>
#include "i_musicinterns.h"
#include "c_cvars.h"
#include <gme/gme.h>
#include <mutex>
#include "v_text.h"
#include "files.h"
#include "templates.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

class GMESong : public StreamSource
{
public:
	GMESong(Music_Emu *emu, int sample_rate);
	~GMESong();
	bool SetSubsong(int subsong) override;
	bool Start() override;
	void ChangeSettingNum(const char *name, double val) override;
	FString GetStats() override;
	bool GetData(void *buffer, size_t len) override;
	SoundStreamInfo GetFormat() override;

protected:
	std::mutex CritSec;
	Music_Emu *Emu;
	gme_info_t *TrackInfo;
	int SampleRate;
	int CurrTrack;
	bool started = false;

	bool StartTrack(int track, bool getcritsec=true);
	bool GetTrackInfo();
	int CalcSongLength();
};

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Currently not used.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
//
// GME_CheckFormat
//
//==========================================================================

const char *GME_CheckFormat(uint32_t id)
{
	return gme_identify_header(&id);
}

//==========================================================================
//
// GME_OpenSong
//
//==========================================================================

StreamSource *GME_OpenSong(FileReader &reader, const char *fmt, float stereo_depth)
{
	gme_type_t type;
	gme_err_t err;
	uint8_t *song;
	Music_Emu *emu;
	int sample_rate;
	
	type = gme_identify_extension(fmt);
	if (type == NULL)
	{
		return NULL;
	}
	sample_rate = (int)GSnd->GetOutputRate();
	emu = gme_new_emu(type, sample_rate);
	if (emu == nullptr)
	{
		return nullptr;
	}

    auto fpos = reader.Tell();
	auto len = reader.GetLength();
    song = new uint8_t[len];
    if (reader.Read(song, len) != len)
    {
        delete[] song;
        gme_delete(emu);
        reader.Seek(fpos, FileReader::SeekSet);
        return nullptr;
    }

	err = gme_load_data(emu, song, (long)len);
    delete[] song;

	if (err != nullptr)
	{
		Printf("Failed loading song: %s\n", err);
		gme_delete(emu);
        reader.Seek(fpos, FileReader::SeekSet);
		return nullptr;
	}
	gme_set_stereo_depth(emu, std::min(std::max(stereo_depth, 0.f), 1.f));
	gme_set_fade(emu, -1); // Enable infinite loop

#if GME_VERSION >= 0x602
	gme_set_autoload_playback_limit(emu, 0);
#endif // GME_VERSION >= 0x602

	return new GMESong(emu, sample_rate);
}

//==========================================================================
//
// GMESong - Constructor
//
//==========================================================================

GMESong::GMESong(Music_Emu *emu, int sample_rate)
{
	Emu = emu;
	SampleRate = sample_rate;
	CurrTrack = 0;
	TrackInfo = NULL;
}


SoundStreamInfo GMESong::GetFormat()
{
	return { 32*1024, SampleRate, -2 };
}

//==========================================================================
//
// GMESong - Destructor
//
//==========================================================================

GMESong::~GMESong()
{
	if (TrackInfo != NULL)
	{
		gme_free_info(TrackInfo);
	}
	if (Emu != NULL)
	{
		gme_delete(Emu);
	}
}


//==========================================================================
//
// GMESong :: GMEDepthChanged
//
//==========================================================================

void GMESong::ChangeSettingNum(const char *name, double val)
{
	if (Emu != nullptr && !stricmp(name, "gme.stereodepth"))
	{
		gme_set_stereo_depth(Emu, clamp((float)val, 0.f, 1.f));
	}
}

//==========================================================================
//
// GMESong :: Play
//
//==========================================================================

bool GMESong::Start()
{
	return StartTrack(CurrTrack);
}


//==========================================================================
//
// GMESong :: SetSubsong
//
//==========================================================================

bool GMESong::SetSubsong(int track)
{
	if (CurrTrack == track)
	{
		return true;
	}
	if (!started)
	{
		CurrTrack = track;
		return true;
	}
	return StartTrack(track);
}

//==========================================================================
//
// GMESong :: StartTrack
//
//==========================================================================

bool GMESong::StartTrack(int track, bool getcritsec)
{
	gme_err_t err;

	if (getcritsec)
	{
		std::lock_guard<std::mutex> lock(CritSec);
		err = gme_start_track(Emu, track);
	}
	else
	{
		err = gme_start_track(Emu, track);
	}
	if (err != NULL)
	{
		Printf("Could not start track %d: %s\n", track, err);
		return false;
	}
	CurrTrack = track;
	GetTrackInfo();
	if (!m_Looping)
	{
		gme_set_fade(Emu, CalcSongLength());
	}
	return true;
}

//==========================================================================
//
// GMESong :: GetStats
//
//==========================================================================

FString GMESong::GetStats()
{
	FString out;

	if (TrackInfo != NULL)
	{
		int time = gme_tell(Emu);
		out.Format(
			"Track: " TEXTCOLOR_YELLOW "%d" TEXTCOLOR_NORMAL
			"  Time:" TEXTCOLOR_YELLOW "%3d:%02d:%03d" TEXTCOLOR_NORMAL
			"  System: " TEXTCOLOR_YELLOW "%s" TEXTCOLOR_NORMAL,
			CurrTrack,
			time/60000,
			(time/1000) % 60,
			time % 1000,
			TrackInfo->system);
	}
	return out;
}

//==========================================================================
//
// GMESong :: GetTrackInfo
//
//==========================================================================

bool GMESong::GetTrackInfo()
{
	gme_err_t err;

	if (TrackInfo != NULL)
	{
		gme_free_info(TrackInfo);
		TrackInfo = NULL;
	}
	err = gme_track_info(Emu, &TrackInfo, CurrTrack);
	if (err != NULL)
	{
		Printf("Could not get track %d info: %s\n", CurrTrack, err);
		return false;
	}
	return true;
}

//==========================================================================
//
// GMESong :: CalcSongLength
//
//==========================================================================

int GMESong::CalcSongLength()
{
	if (TrackInfo == NULL)
	{
		return 150000;
	}
	if (TrackInfo->length > 0)
	{
		return TrackInfo->length;
	}
	if (TrackInfo->loop_length > 0)
	{
		return TrackInfo->intro_length + TrackInfo->loop_length * 2;
	}
	return 150000;
}

//==========================================================================
//
// GMESong :: Read													STATIC
//
//==========================================================================

bool GMESong::GetData(void *buffer, size_t len)
{
	gme_err_t err;

	std::lock_guard<std::mutex> lock(CritSec);
	if (gme_track_ended(Emu))
	{
		if (m_Looping)
		{
			StartTrack(CurrTrack, false);
		}
		else
		{
			memset(buffer, 0, len);
			return false;
		}
	}
	err = gme_play(Emu, int(len >> 1), (short *)buffer);
	return (err == NULL);
}

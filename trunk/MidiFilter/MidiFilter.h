// Copyleft 2021 Chris Korda
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or any later version.
/*
        chris korda
 
		revision history:
		rev		date	comments
        00      13jan21	initial version

*/

#pragma once

#include "Midi.h"
#include "MidiFile.h"

class CMidiFilter {
public:
// Construction
	CMidiFilter();

// Types
	class CMidiEvent {
	public:
		CMidiEvent() {};
		CMidiEvent(DWORD nTime, DWORD dwEvent, int iOther = -1, int nDur = 0);
		bool	operator==(const CMidiEvent &evt) const;
		bool	operator!=(const CMidiEvent &evt) const;
		bool	operator<(const CMidiEvent &evt) const;
		bool	operator>(const CMidiEvent &evt) const;
		bool	operator<=(const CMidiEvent &evt) const;
		bool	operator>=(const CMidiEvent &evt) const;
		DWORD	m_nTime;	// event time in ticks
		MIDI_MSG	m_evt;	// MIDI event structure
		int		m_iOther;	// for paired note on/off events, index of other event, otherwise -1
		int		m_nDur;		// for note on events, note's duration in ticks (requires CreateNoteDurations)
	};
	typedef CArrayEx<CMidiEvent, CMidiEvent&> CMidiEventArray;
	typedef CArrayEx<CMidiEventArray, CMidiEventArray&> CMidiTrackArray;

// Operations
	void	Read(LPCTSTR pszPath);
	void	Write(LPCTSTR pszPath);
	void	DumpTrack(int iTrack) const;
	void	DumpTracks() const;
	void	TieAdjacent(int iTrack);
	void	TieAdjacent(int iFirstTrack, int nTracks);
	void	Merge(int iFirstTrack, int nTracks);
	void	OffsetDurations(int iTrack, int nOffset);
	void	MakeLegato(int iTrack);
	void	MakeLegato(int iFirstTrack, int nTracks);
	void	AddPauses(int nPauseTicks);
	void	Quantize(int nQuant, int nSwing = 0);
	void	CreateNoteDurations();
	void	ApplyNoteDurations();
	void	ScaleNoteDurations(double fScale);
	void	WritePitchClassSets(LPCTSTR pszPath, int nQuant) const;
	void	ApplyGroove(int nLoopLen, int nOffsetSpan, int nVelSpan = 0);
	static	CString PCSToString(WORD nPCS, TCHAR cSeparator = ' ');

protected:
// Constants
	enum {
		OCTAVE = 12,
	};

// Member vars
	CMidiTrackArray	m_arrTrack;	// array of tracks
	CStringArrayEx	m_arrTrackName;	// array of track names
	CMidiFile::TIME_SIGNATURE	m_sigTime;	// time signature; initialized to zero
	CMidiFile::KEY_SIGNATURE	m_sigKey;	// key signature; initialized to 0xff
	CMidiFile::CMidiEventArray	m_arrTempoMap;	// tempo map
	USHORT	m_nPPQ;				// time division in ticks
	UINT	m_nTempo;			// tempo in microseconds per quarter note
	UINT	m_nSongDuration;	// duration of song in ticks
	bool	m_bHasNoteDurations;	// true if note durations are valid
};

inline CMidiFilter::CMidiEvent::CMidiEvent(DWORD nTime, DWORD dwEvent, int iOther, int nDur)
{
	m_nTime = nTime;
	m_evt.dw = dwEvent;
	m_iOther = iOther;
	m_nDur = nDur;
}

inline bool CMidiFilter::CMidiEvent::operator==(const CMidiEvent &evt) const
{
	return m_nTime == evt.m_nTime && m_evt.dw == evt.m_evt.dw;
}

inline bool CMidiFilter::CMidiEvent::operator!=(const CMidiEvent &evt) const
{
	return m_nTime != evt.m_nTime || m_evt.dw != evt.m_evt.dw;
}

inline bool CMidiFilter::CMidiEvent::operator<(const CMidiEvent &evt) const
{
	return m_nTime < evt.m_nTime || (m_nTime == evt.m_nTime && m_evt.dw < evt.m_evt.dw);
}

inline bool CMidiFilter::CMidiEvent::operator>(const CMidiEvent &evt) const
{
	return m_nTime > evt.m_nTime || (m_nTime == evt.m_nTime && m_evt.dw > evt.m_evt.dw);
}

inline bool CMidiFilter::CMidiEvent::operator<=(const CMidiEvent &evt) const
{
	return m_nTime < evt.m_nTime || (m_nTime == evt.m_nTime && m_evt.dw <= evt.m_evt.dw);
}

inline bool CMidiFilter::CMidiEvent::operator>=(const CMidiEvent &evt) const
{
	return m_nTime > evt.m_nTime || (m_nTime == evt.m_nTime && m_evt.dw >= evt.m_evt.dw);
}

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

// MidiProc.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "MidiFilter.h"
#include "MidiFile.h"
#include "Midi.h"

#define _USE_MATH_DEFINES
#include <math.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// The one and only application object

CWinApp theApp;

using namespace std;

CMidiFilter::CMidiFilter()
{
	ZeroMemory(&m_sigTime, sizeof(m_sigTime));
	memset(&m_sigKey, 0xff, sizeof(m_sigKey));
	m_nPPQ = 0;
	m_nTempo = 0;
	m_nSongDuration = 0;
	m_bHasNoteDurations = false;
}

void CMidiFilter::Read(LPCTSTR pszPath)
{
	CMidiFile	fMidi(pszPath, CFile::modeRead);
	CMidiFile::CMidiTrackArray	arrTrack;
	fMidi.ReadTracks(arrTrack, m_arrTrackName, m_nPPQ, &m_nTempo, &m_sigTime, &m_sigKey, &m_arrTempoMap);
	int	nTracks = arrTrack.GetSize();
	m_arrTrack.SetSize(nTracks);
	int	arrNoteOn[MIDI_CHANNELS][MIDI_NOTES];
	memset(arrNoteOn, -1, sizeof(arrNoteOn));
	for (int iTrack = 0; iTrack < nTracks; iTrack++) {
		const CMidiFile::CMidiEventArray&	arrInEvt = arrTrack[iTrack];
		CMidiEventArray&	arrOutEvt = m_arrTrack[iTrack];
		int	nTime = 0;
		int	nEvts = arrInEvt.GetSize();
		arrOutEvt.SetSize(nEvts);
		for (int iEvt = 0; iEvt < nEvts; iEvt++) {
			const CMidiFile::MIDI_EVENT&	evtIn = arrInEvt[iEvt];
			CMidiEvent&	evtOut = arrOutEvt[iEvt];
			nTime += evtIn.DeltaT;
			MIDI_MSG	msg;
			msg.dw = evtIn.Msg;
			evtOut.m_evt = msg;
			evtOut.m_nTime = nTime;
			evtOut.m_iOther = -1;
			evtOut.m_nDur = 0;
			int	nCmd = MIDI_CMD(msg.dw);
			if (nCmd == NOTE_ON || nCmd == NOTE_OFF) {
				int	iChan = msg.chan;
				int	iNote = msg.p1;
				if (!msg.p2 || nCmd == NOTE_OFF) {	// if note off
					if (arrNoteOn[iChan][iNote] >= 0) {
						int	iNoteOn = arrNoteOn[iChan][iNote];
						arrOutEvt[iNoteOn].m_iOther = iEvt;
						arrOutEvt[iEvt].m_iOther = iNoteOn;
						arrNoteOn[iChan][iNote] = -1;
					} else {
						printf("unpaired note off in track %d, event %d: time %d\n", iTrack, iEvt, nTime);
					}
				} else {	// note on
					if (arrNoteOn[iChan][iNote] >= 0) {
						printf("note overlap in track %d, event %d: time %d\n", iTrack, iEvt, nTime);
						AfxThrowNotSupportedException();
					} else {
						arrNoteOn[iChan][iNote] = iEvt;
					}
				}
			}
		}
	}
	m_nSongDuration = 0;
	int	nTempoEvts = m_arrTempoMap.GetSize();
	if (nTempoEvts) {
		for (int iEvt = 0; iEvt < nTempoEvts; iEvt++) {
			m_nSongDuration += m_arrTempoMap[iEvt].DeltaT;
			m_arrTempoMap[iEvt].DeltaT = m_nSongDuration;	// convert time from delta to absolute
		}
		if (nTempoEvts == 2 && m_arrTempoMap[0].Msg == m_arrTempoMap[1].Msg)
			m_arrTempoMap.RemoveAll();	// default tempo map doesn't count
		else if (!m_arrTempoMap[0].DeltaT && m_arrTempoMap[0].Msg == m_nTempo)
			m_arrTempoMap.RemoveAt(0);	// first entry is primary tempo
	}
}

void CMidiFilter::Write(LPCTSTR pszPath)
{
	if (m_bHasNoteDurations)
		ApplyNoteDurations();
	CMidiFile	fMidi(pszPath, CFile::modeWrite | CFile::modeCreate);
	double	fTempo = CMidiFile::MICROS_PER_MINUTE / double(m_nTempo);
	const CMidiFile::KEY_SIGNATURE*	pSigKey;
	if (m_sigKey.IsMinor != 0xff)	// if valid key signature was read
		pSigKey = &m_sigKey;
	else
		pSigKey = NULL;
	const CMidiFile::TIME_SIGNATURE* pSigTime;
	if (m_sigTime.Numerator)	// if valid time signature was read
		pSigTime = &m_sigTime;
	else
		pSigTime = NULL;
	const CMidiFile::CMidiEventArray* pTempoMap;
	if (!m_arrTempoMap.IsEmpty()) {
		int	nTempoEvts = m_arrTempoMap.GetSize();
		int	nPrevTime = 0;
		for (int iEvt = 0; iEvt < nTempoEvts; iEvt++) {
			int	nTime = m_arrTempoMap[iEvt].DeltaT;
			m_arrTempoMap[iEvt].DeltaT = nTime - nPrevTime;	// convert time from absolute to delta
			nPrevTime = nTime;
		}
		pTempoMap = &m_arrTempoMap;
	} else
		pTempoMap = NULL;
	int	nTracks = m_arrTrack.GetSize();
	bool	bHaveSongTrack = nTracks && m_arrTrack[0].IsEmpty() && m_arrTrackName[0].IsEmpty();
	int	nOutTracks = bHaveSongTrack ? nTracks - 1 : nTracks;
	int	iFirstTrack = bHaveSongTrack;
	fMidi.WriteHeader(nOutTracks, m_nPPQ, fTempo, m_nSongDuration, pSigTime, pSigKey, pTempoMap);
	CMidiFile::CMidiEventArray	arrOutEvt;
	for (int iTrack = iFirstTrack; iTrack < nTracks; iTrack++) {
		const CMidiEventArray& arrInEvt = m_arrTrack[iTrack];
		int	nEvts = arrInEvt.GetSize();
		arrOutEvt.SetSize(nEvts);
		int	nPrevTime = 0;
		for (int iEvt = 0; iEvt < nEvts; iEvt++) {
			CMidiFile::MIDI_EVENT	evt;
			evt.Msg = arrInEvt[iEvt].m_evt.dw;
			int	nTime = arrInEvt[iEvt].m_nTime;
			evt.DeltaT = nTime - nPrevTime;
			nPrevTime = nTime;
			arrOutEvt[iEvt] = evt;
		}
		fMidi.WriteTrack(arrOutEvt, m_arrTrackName[iTrack]);
	}
}

void CMidiFilter::DumpTrack(int iTrack) const
{
	const CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
	int	nEvts = arrEvt.GetSize();
	for (int iEvt = 0; iEvt < nEvts; iEvt++) {
		const CMidiEvent&	evt = arrEvt[iEvt];
		printf("%d: %d %d %d %d %d %d %d\n", iEvt, evt.m_nTime, evt.m_evt.cmd, evt.m_evt.chan, evt.m_evt.p1, evt.m_evt.p2, evt.m_iOther, evt.m_nDur);
	}
}

void CMidiFilter::DumpTracks() const
{
	if (!m_arrTempoMap.IsEmpty()) {
		printf("tempo map\n");
		int	nEvts = m_arrTempoMap.GetSize();
		for (int iEvt = 0; iEvt < nEvts; iEvt++) {
			const CMidiFile::MIDI_EVENT&	evt = m_arrTempoMap[iEvt];
			printf("%d: %d %d\n", iEvt, evt.DeltaT, evt.Msg);
		}
	}
	for (int iTrack = 0; iTrack < m_arrTrack.GetSize(); iTrack++) {
		printf("track %d\n", iTrack);
		DumpTrack(iTrack);
	}
}

void CMidiFilter::TieAdjacent(int iTrack)
{
	CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
	int	nTies = 0;
	int	nEvts = arrEvt.GetSize();
	int	iEvt;
	for (iEvt = 0; iEvt < nEvts; iEvt++) {
		CMidiEvent&	evt = arrEvt[iEvt];
		if (MIDI_CMD(evt.m_evt.dw) == NOTE_ON && evt.m_evt.p2) {
			int	nTime = evt.m_nTime;
			int	iOther = iEvt - 1;
			while (iOther >= 0 && arrEvt[iOther].m_nTime == nTime) {
				CMidiEvent&	evtOther = arrEvt[iOther];
				if (MIDI_CMD(evtOther.m_evt.dw) == NOTE_ON
				&& evtOther.m_evt.chan == evt.m_evt.chan 
				&& evtOther.m_evt.p1 == evt.m_evt.p1
				&& !evtOther.m_evt.p2) {	// matching note off
					int	iNoteOff = evt.m_iOther;
					if (iNoteOff >= 0) {
						int	iNoteOn = evtOther.m_iOther;
						evt.m_evt.dw = -1;
						evtOther.m_evt.dw = -1;
						arrEvt[iNoteOn].m_iOther = iNoteOff;
						arrEvt[iNoteOff].m_iOther = iNoteOn;
						nTies++;
					}
				}
				iOther--;
			}
		}
	}
	for (iEvt = nEvts - 1; iEvt >= 0; iEvt--) {
		if (arrEvt[iEvt].m_evt.dw == -1)
			arrEvt.RemoveAt(iEvt);
	}
	printf("nEvts=%d nTies=%d\n", nEvts, nTies);
}

void CMidiFilter::TieAdjacent(int iFirstTrack, int nTracks)
{
	int	iEndTrack = iFirstTrack + nTracks;
	for (int iTrack = iFirstTrack; iTrack < iEndTrack; iTrack++) {
		TieAdjacent(iTrack);
	}
}

void CMidiFilter::Merge(int iFirstTrack, int nTracks)
{
	CMidiEventArray	arrDest;
	int	iTrack;
	int	iEndTrack = iFirstTrack + nTracks;
	for (iTrack = iFirstTrack; iTrack < iEndTrack; iTrack++) {
		CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
		int	nEvts = arrEvt.GetSize();
		for (int iEvt = 0; iEvt < nEvts; iEvt++) {
			CMidiEvent&	evt = arrEvt[iEvt];
			if (MIDI_CMD(evt.m_evt.dw) == NOTE_ON && !evt.m_evt.p2) {
				evt.m_evt.chan = 0;
				arrDest.InsertSorted(evt);
			}
		}
	}
	for (iTrack = iFirstTrack; iTrack < iEndTrack; iTrack++) {
		CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
		int	nEvts = arrEvt.GetSize();
		for (int iEvt = 0; iEvt < nEvts; iEvt++) {
			CMidiEvent&	evt = arrEvt[iEvt];
			if (MIDI_CMD(evt.m_evt.dw) == NOTE_ON && evt.m_evt.p2) {
				evt.m_evt.chan = 0;
				arrDest.InsertSorted(evt);
			}
		}
	}
	bool	arrNoteRefs[MIDI_CHANNELS][MIDI_NOTES] = {0};
	int	nEvts = arrDest.GetSize();
	int	iEvt = 0;
	while (iEvt < nEvts) {
		CMidiEvent&	evt = arrDest[iEvt];
		if (MIDI_CMD(evt.m_evt.dw) == NOTE_ON) {
			int	iChan = evt.m_evt.chan;
			int	iNote = evt.m_evt.p1;
			if (evt.m_evt.p2) {
				if (arrNoteRefs[iChan][iNote]) {
					CMidiEvent	evtOff = evt;
					evtOff.m_evt.p2 = 0;
					printf("collision %d: %d\n", iEvt, evt.m_nTime);
					for (int iOff = iEvt + 1; iOff < nEvts; iOff++) {
						if (!memcmp(&arrDest[iOff].m_evt, &evtOff.m_evt, sizeof(MIDI_MSG))) {
							printf("off %d: %d\n", iOff, arrDest[iOff].m_nTime);
							arrDest.InsertAt(iEvt, evtOff);
							arrDest.RemoveAt(iOff + 1);
							iEvt++;
							break;
						}
					}
				}
				arrNoteRefs[iChan][iNote] = true;
			} else {
				arrNoteRefs[iChan][iNote] = false;
			}
		}
		iEvt++;
	}	
	m_arrTrack.RemoveAt(iFirstTrack + 1, nTracks - 1);
	m_arrTrackName.RemoveAt(iFirstTrack + 1, nTracks - 1);
	m_arrTrack[iFirstTrack] = arrDest;
}

void CMidiFilter::OffsetDurations(int iTrack, int nOffset)
{
	CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
	int	nTies = 0;
	int	nEvts = arrEvt.GetSize();
	int	iEvt;
	int	nChanges = 0;
	for (iEvt = 0; iEvt < nEvts; iEvt++) {
		CMidiEvent&	evt = arrEvt[iEvt];
		if (MIDI_CMD(evt.m_evt.dw) == NOTE_ON && !evt.m_evt.p2) {
			evt.m_nTime += nOffset;
			nChanges++;
		}
	}
	printf("%d changes\n", nChanges);
}

void CMidiFilter::MakeLegato(int iTrack)
{
	CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
	int	nTies = 0;
	int	nEvts = arrEvt.GetSize();
	int	iEvt;
	int	nChanges = 0;
	int	iPrevNote = -1;
	for (iEvt = 0; iEvt < nEvts; iEvt++) {
		CMidiEvent&	evt = arrEvt[iEvt];
		if (MIDI_CMD(evt.m_evt.stat) == NOTE_ON && evt.m_evt.p2) {
			if (iPrevNote >= 0) {
				CMidiEvent&	evtPrev = arrEvt[iPrevNote];
				int	nDelta = evtPrev.m_nTime + evtPrev.m_nDur - evt.m_nTime;
				if (abs(nDelta) < evtPrev.m_nDur) {
					evtPrev.m_nDur = evt.m_nTime - evtPrev.m_nTime;
					nChanges++;
				}
			}
			iPrevNote = iEvt;
		}
	}
	printf("%d changes\n", nChanges);
}

void CMidiFilter::MakeLegato(int iFirstTrack, int nTracks)
{
	int	iEndTrack = iFirstTrack + nTracks;
	for (int iTrack = iFirstTrack; iTrack < iEndTrack; iTrack++) {
		MakeLegato(iTrack);
	}
}

#define GATHERING_INTO_FORM 0

void CMidiFilter::AddPauses(int nPauseTicks)
{
	int	nTracks = m_arrTrack.GetSize();
	for (int iTrack = 1; iTrack < nTracks; iTrack++) {	// for each track
		CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
		int	nEvts = arrEvt.GetSize();
		int	nTempoEvts = m_arrTempoMap.GetSize();
		int	iTempoEvt = 0;
		int	nTempoTime = 0;
		int	nTempo = m_nTempo;
		int	nCumulativeOffset = 0;
		for (int iEvt = 0; iEvt < nEvts; iEvt++) {	// for each of track's events
			CMidiEvent&	evt = arrEvt[iEvt];
			// iterate tempo map elements up to current event time
			while (iTempoEvt < nTempoEvts && evt.m_nTime >= m_arrTempoMap[iTempoEvt].DeltaT) {
				nTempoTime = m_arrTempoMap[iTempoEvt].DeltaT;
				nTempo = m_arrTempoMap[iTempoEvt].Msg;
				iTempoEvt++;
				if (nTempo != m_nTempo) {	// if not song tempo, assume positive transition
#if GATHERING_INTO_FORM 
					// multiple tempo pauses of different lengths are used, so special handling is needed
					switch (nTempo) {
					case 4500234:
						nPauseTicks = 240;	// 1/16 -> 9/16; log(9) / log(2) = 317%; add 1/2
						break;
					case 2496661:
						nPauseTicks = 120;	// 1/16 -> 5/16; log(5) / log(2) = 232%; add 1/4
						break;
					case 6498019:
						nPauseTicks = 360;	// 1/16 -> 13/16; log(13) / log(2) = 370%; add 3/4
						break;
					}
//printf("%d %f\n", nTempo, CMidiFile::MICROS_PER_MINUTE / double(nTempo));
#endif
					nCumulativeOffset += nPauseTicks;	// bump offset to create pause
				}
			}
			if (nTempo != m_nTempo) {	// if during pause
//				ASSERT(evt.m_evt.cmd == (NOTE_ON >> 4) && evt.m_evt.p2 == 0);	// note offs only
				evt.m_nTime -= nPauseTicks;	// for 444 balanced Gray std dev 1.45774 simpler arp 8/23/2025 to avoid duration change
			}
			evt.m_nTime += nCumulativeOffset;
		}
	}
	m_arrTempoMap.SetSize(1);	// delete tempo map
	m_arrTempoMap[0].Msg = m_nTempo;
}

void CMidiFilter::Quantize(int nQuant, int nSwing)
{
	ASSERT(nQuant > 0);
	ASSERT(abs(nSwing) < nQuant);
	int	nTracks = m_arrTrack.GetSize();
	for (int iTrack = 1; iTrack < nTracks; iTrack++) {	// for each track
		CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
		int	nEvts = arrEvt.GetSize();
		for (int iEvt = 0; iEvt < nEvts; iEvt++) {
			CMidiEvent&	evt = arrEvt[iEvt];
			if (MIDI_CMD(evt.m_evt.dw) == NOTE_ON || MIDI_CMD(evt.m_evt.dw) == NOTE_OFF) {
				DWORD	t = evt.m_nTime;
				t += nQuant / 2;
				DWORD	nProd = t / nQuant;
				t -= t % nQuant;
				if (nProd & 1) {
					t += nSwing;
				}
				evt.m_nTime = t;
			}
		}
		arrEvt.Sort();
//		DumpTrack(iTrack);
	}
}

void CMidiFilter::CreateNoteDurations()
{
	ASSERT(!m_bHasNoteDurations);
	// also deletes all note off messages and resets m_iOther
	int	nTracks = m_arrTrack.GetSize();
	for (int iTrack = 1; iTrack < nTracks; iTrack++) {	// for each track
		CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
		int	nEvts = arrEvt.GetSize();
		for (int iEvt = 0; iEvt < nEvts; iEvt++) {
			CMidiEvent&	evt = arrEvt[iEvt];
			if (MIDI_CMD(evt.m_evt.dw) == NOTE_ON) {
				if (evt.m_evt.p2) {	// if note on message
					evt.m_nDur = arrEvt[evt.m_iOther].m_nTime - evt.m_nTime;	// set duration
					ASSERT(evt.m_nDur > 0);
					evt.m_iOther = -1;	// invalidate other index, as note offs will be deleted
				}
			}
		}
		for (int iEvt = nEvts - 1; iEvt >= 0; iEvt--) {	// reverse iterate for stable deletion
			CMidiEvent&	evt = arrEvt[iEvt];
			if ((MIDI_CMD(evt.m_evt.dw) == NOTE_ON && !evt.m_evt.p2) || MIDI_CMD(evt.m_evt.dw) == NOTE_OFF) {
				arrEvt.RemoveAt(iEvt);	// delete note off message
			}
		}
	}
	m_bHasNoteDurations = true;
}

void CMidiFilter::ApplyNoteDurations()
{
	ASSERT(m_bHasNoteDurations);
	int	nTracks = m_arrTrack.GetSize();
	for (int iTrack = 1; iTrack < nTracks; iTrack++) {	// for each track
		CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
		int	nEvts = arrEvt.GetSize();
		int	iEvt = 0;
		while (iEvt < nEvts) {
			CMidiEvent&	evt = arrEvt[iEvt];
			if (MIDI_CMD(evt.m_evt.dw) == NOTE_ON && evt.m_evt.p2) {	// if note on message
				CMidiEvent	evtNoteOff(evt.m_nTime + evt.m_nDur, evt.m_evt.dw);	// offset event time by note duration
				evtNoteOff.m_evt.p2 = 0;	// make note off by zeroing velocity
				arrEvt.InsertSorted(evtNoteOff);	// add event, maintaining order
				nEvts++;	// bump event count
			}
			iEvt++;	// bump event index
		}
	}
	m_bHasNoteDurations = false;
}

void CMidiFilter::ScaleNoteDurations(double fScale)
{
	ASSERT(fScale > 0);
	ASSERT(m_bHasNoteDurations);
	int	nTracks = m_arrTrack.GetSize();
	for (int iTrack = 1; iTrack < nTracks; iTrack++) {	// for each track
		CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
		int	nEvts = arrEvt.GetSize();
		for (int iEvt = 0; iEvt < nEvts; iEvt++) {
			CMidiEvent&	evt = arrEvt[iEvt];
			if (MIDI_CMD(evt.m_evt.dw) == NOTE_ON) {
				evt.m_nDur = Round(evt.m_nDur * fScale);
			}
		}
	}
}

CString CMidiFilter::PCSToString(WORD nPCS, TCHAR cSeparator)
{
	CString s, t;
	for (int iPC = 0; iPC < OCTAVE; iPC++) {
		DWORD	nMask = (0x1 << iPC);
		if (nPCS & nMask) {
			if (!s.IsEmpty())
				s += cSeparator;
			t.Format(_T("%x"), iPC);
			s += t;
		}
	}
	return s;
}

void CMidiFilter::WritePitchClassSets(LPCTSTR pszPath, int nQuant) const
{
	ASSERT(m_bHasNoteDurations);
	int	nTracks = m_arrTrack.GetSize();
	int	nSongChords = m_nSongDuration / nQuant;
	CWordArray	arrPCS;
	arrPCS.SetSize(nSongChords);
	struct NOTE_MASK {
		ULONGLONG	a[2];
	};
	CArray<NOTE_MASK, NOTE_MASK> arrNoteMask;
	arrNoteMask.SetSize(nSongChords);
	for (int iTrack = 1; iTrack < nTracks; iTrack++) {	// for each track
		const CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
		for (int iEvt = 0; iEvt < arrEvt.GetSize(); iEvt++) {
			const CMidiEvent&	evt = arrEvt[iEvt];
			if (evt.m_nDur) {
				int	iChordStart = evt.m_nTime / nQuant;
				int	iChordEnd = iChordStart + (evt.m_nDur / nQuant);
				iChordEnd = min(iChordEnd, nSongChords);
				int	iPC = evt.m_evt.p1 % OCTAVE;
				WORD	nPCSMask = 1 << iPC;
				int	iNoteWord = evt.m_evt.p1 / (MIDI_NOTES / 2);
				ULONGLONG	nNoteMask = 1ull << (evt.m_evt.p1 % (MIDI_NOTES / 2));
				for (int iChord = iChordStart; iChord < iChordEnd; iChord++) {
					arrPCS[iChord] |= nPCSMask;
					arrNoteMask[iChord].a[iNoteWord] |= nNoteMask;
				}
			}
		}
	}
	int	nErrors = 0;
	for (int iChord = 0; iChord < nSongChords; iChord++) {
		WORD	nPCS = arrPCS[iChord];
		int	nUniqueTones = 0;
		for (int iPC = 0; iPC < OCTAVE; iPC++) {
			if (nPCS & (1 << iPC)) {
				nUniqueTones++;
			}
		}
		int	nChordSize = 0;
		for (int iNoteWord = 0; iNoteWord < 2; iNoteWord++) {
			ULONGLONG	nNoteMask = arrNoteMask[iChord].a[iNoteWord];
			for (int iNote = 0; iNote < MIDI_NOTES / 2; iNote++) {
				if (nNoteMask & (1ull << iNote)) {
					nChordSize++;
				}
			}
		}
		if (nUniqueTones && nUniqueTones != nChordSize) {
			printf("%d doubled tones at %d\n", nChordSize - nUniqueTones, iChord + 1);
			nErrors++;
		}
	}
	for (int iChord = 0; iChord < nSongChords; iChord++) {
		WORD	nPCS = arrPCS[iChord];
		nPCS = nPCS | (nPCS << OCTAVE);	// repeat an octave up
		int	nChromaticClusters = 0;
		for (int iPC = 0; iPC < OCTAVE; iPC++) {
			WORD	nMask = (0x7 << iPC);
			if ((nPCS & nMask) == nMask) {
				nChromaticClusters++;
			}
		}
		if (nChromaticClusters) {
			CString s(PCSToString(nPCS));
			s.Insert(0, '[');
			s += ']';
			_tprintf(_T("%d chromatic clusters in %s at %d\n"), nChromaticClusters, s, iChord + 1);
			nErrors++;
		}
	}
	CStdioFile	fOut(pszPath, CFile::modeWrite | CFile::modeCreate);
	for (int iChord = 0; iChord < nSongChords; iChord++) {
		WORD	nPCS = arrPCS[iChord];
		CString sLine(PCSToString(nPCS, ','));
		fOut.WriteString(sLine + '\n');
	}
	if (!nErrors)
		printf("all good\n");
}

void CMidiFilter::ApplyGroove(int nLoopLen, int nOffsetSpan, int nVelSpan)
{
	ASSERT(m_bHasNoteDurations);
	int	nTracks = m_arrTrack.GetSize();
	int	nVelBase = 100;
	for (int iTrack = 1; iTrack < nTracks; iTrack++) {	// for each track
		CMidiEventArray&	arrEvt = m_arrTrack[iTrack];
		for (int iEvt = 0; iEvt < arrEvt.GetSize(); iEvt++) {
			CMidiEvent&	evt = arrEvt[iEvt];
			if (evt.m_nDur) {
				double	fPos = evt.m_nTime % nLoopLen / double(nLoopLen);
				double	fExp = (fPos < 0.5) ? 0.5 - fPos : fPos - 0.5;
				double	fVal = pow(2, fExp * 2) - 1;
				int	nOffset = Round(fVal * nOffsetSpan);
				evt.m_nTime += nOffset;
				// leave velocities alone for now
/*				fExp = (fPos < 0.5) ? fPos : 1 - fPos;
				fVal = pow(2, fExp * 2) - 1;
				int	nVel = nVelBase - nVelSpan + Round(fVal * nVelSpan);
				evt.m_evt.p2 = CLAMP(nVel, 1, 127);
				*/
			}
		}
	}
}


void test()
{
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\Forgive the Night vox rec1.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\Roro shorter ritard.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\RingGray ABC merged.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\BG 3232 scales 2 rec1 multichan.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\string and piano.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\Music\\scores\\LilyPond\\test\\Style Is Feeling pre-filter.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\SetConsonance 2332 bass turn.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\guitar vamp with mute 2 rec 1d.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\guitar vamp with mute 2 rec 1e dub arp end.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\BG 22322 + 22222.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\Music\\scores\\LilyPond\\input\\Gathering Into Form tempo pauses.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\BG 22322 + 22222.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\Documents\\balanced Gray code paper\\supplements\\444_balanced_Gray_std_dev.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\Music\\scores\\LilyPond\\new\\534 balanced Gray short spans drop 2 waltz bass.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\444 balanced Gray std dev 1.45774 simpler arp non-triplet.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\444 balanced Gray std dev 1.45774 groove.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\444 balanced Gray std dev 1.45774 simpler bass 3.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\444 balanced Gray std dev 1.45774 intro NN-XT.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\534 BG single channel.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\Music\\scores\\LilyPond\\input\\Facts in Dispute score.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\Music\\scores\\LilyPond\\new\\BGscore22222.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\Sense in Depth single channel.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\Music\\scores\\LilyPond\\new\\534 balanced Gray short spans drop 2 waltz melody Reason mix 002 ending SCORE.mid"); // title: Renumbering
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\Fine Teeth A single channel.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\Fine Teeth B piano 4 chan velo Rad Piano.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\Fine Teeth C single channel.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\Fine Teeth C piano.mid");
	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\Fine Teeth C piano hexachord.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\debug\\Fine Teeth C piano test.mid");
//	LPCTSTR	pszSrcPath = _T("C:\\Chris\\MyProjects\\Polymeter\\docs\\test\\Fine Teeth C piano scale waltz slight hiccup multichan.mid");
	CMidiFilter	mf;
	mf.Read(pszSrcPath);
//	mf.DumpTrack(1);
#if 0	// for string and piano (AKA Hard Muscle, Fine Skin)
	mf.TieAdjacent(1, 5);
	mf.Merge(2, 4);
#endif
#if 0	// for BG 3232 scales 2 rec1 ending (AKA Style is Feeling)
	mf.TieAdjacent(1, 6);	
#endif
#if 0
	mf.OffsetDurations(1, -1);	// fix intentional +1 durations, purpose of which is merging of overlapped notes
#endif
#if 0	// replace tempo rests with delays
	const int nPauseTicks = 60;
	mf.AddPauses(nPauseTicks);
#endif
#if 0	// for BG 22322 + 22222
	mf.Quantize(240, 0);
	mf.CreateNoteDurations();
//	mf.WritePitchClassSets(_T("Facts in Dispute PCS.txt"), 240); // lots of chromatic clusters (even 4-note ones), and doublings too
//	mf.WritePitchClassSets(_T("Sense in Depth PCS.txt"), 240); // no chromatic clusters, but a few doublings
//	mf.WritePitchClassSets(_T("Fine Teeth A.txt"), 240); // no chromatic clusters, no doublings (excluding intro and outro)
//	mf.WritePitchClassSets(_T("Fine Teeth C.txt"), 240); // no chromatic clusters, no doublings (excluding intro and outro)
//	mf.WritePitchClassSets(_T("Fine Teeth C piano.txt"), 240);
	mf.WritePitchClassSets(_T("Fine Teeth C piano hexachord.txt"), 240);
#endif
#if 0	// for 534 balanced Gray short spans drop 2 waltz bass (Renumbering)
	mf.Quantize(120, 0);
	mf.CreateNoteDurations();
	mf.WritePitchClassSets(_T("Renumbering.txt"), 120); // some doublings
#endif
#if 0	// 444 balanced Gray std dev 1.45774 simpler bass 3 (Fine Teeth)  8/27/2025
	mf.Quantize(240, 0);
	mf.CreateNoteDurations();
	mf.WritePitchClassSets(_T("444 PCS check 2.txt"), 240);
#endif
#if 0	// Fine Teeth B piano 4 chan
	mf.AddPauses(60);
	mf.CreateNoteDurations();
	mf.ApplyGroove(25 * 60, 10);
	mf.MakeLegato(1, 4);
	mf.ApplyNoteDurations();
	mf.Merge(1, 4);	// disable merge to create score
#endif
#if 0	// Fine Teeth C piano multichan
	mf.CreateNoteDurations();
	mf.ApplyGroove(12 * 60, 10);
	mf.MakeLegato(1, 5);
	mf.ApplyNoteDurations();
	mf.Merge(1, 6);	// disable merge to create score
#endif
#if 1	// Fine Teeth C piano multichan
	mf.CreateNoteDurations();
	mf.WritePitchClassSets(_T("Fine Teeth C hexachord.txt"), 720);
#endif
//	mf.DumpTracks();
	mf.Write(_T("output.mid"));
	printf("done\n");
	fgetc(stdin);
}

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	HMODULE hModule = ::GetModuleHandle(NULL);

	if (hModule != NULL)
	{
		// initialize MFC and print and error on failure
		if (!AfxWinInit(hModule, NULL, ::GetCommandLine(), 0))
		{
			// TODO: change error code to suit your needs
			_tprintf(_T("Fatal Error: MFC initialization failed\n"));
			nRetCode = 1;
		}
		else
		{
			test();
		}
	}
	else
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: GetModuleHandle failed\n"));
		nRetCode = 1;
	}

	return nRetCode;
}

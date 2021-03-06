
#include <wx/wxprec.h>		//use precompiled wx headers unless compiler does not support precompilation
#ifndef WX_PRECOMP
    #include <wx/wx.h>
#endif
#include <wx/file.h>
#include <wx/listctrl.h>

#include "ht2util.h"



//get available savestate memory
wxInt16 mainFrame::getFreeMem() {

	wxInt16 freeMem = htsize - (getFirstFree() - baseDiff) - 4;
	if (legacyFileEnd) freeMem -= 2;

	return freeMem;
}


//get first free mem address
unsigned mainFrame::getFirstFree() {

	unsigned firstFree = lutOffset + baseDiff + 32;
	
	for (int i = 0; i < 8; i++) {
	
		if (statebeg[i]+ statelen[i] > firstFree) firstFree = statebeg[i] + statelen[i] + 1;
		
	}

	return firstFree;
	
}



//check if there are empty save slots available
bool mainFrame::isEmptyStateAvailable() {

	bool emptyStateAvailable = false;
	
	for (int i=0; i<8; i++) {
	
		if (statelen[i] == 0) emptyStateAvailable = true;
		
	}
	
	if (!emptyStateAvailable) {
	
		wxMessageDialog error0(NULL, wxT("Error: No free savestate slots available.\nTry deleting something first."),
		   wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		   
		error0.ShowModal();
		
	}
	
	return emptyStateAvailable;
	
}



unsigned mainFrame::getBaseDiff(int model, int baseOffset) {

	const unsigned basediff[3] = { 0x9104, 0x932b, 0x9d99 };	
	unsigned diff = basediff[model] - baseOffset + 5;
	
	return diff;
	
}



//returns the first file position after the internal file name
int mainFrame::getBaseOffset(wxUint8 *htdata) {

	const char vstr[5] = { 0x48, 0x54, 0x20, 0x32, 0x2e };	//"HT 2."
	int vno = 0;
	int fileoffset = 0x40;
	bool foundPrgmHeader = false;

	while ((!foundPrgmHeader) && (fileoffset < 0x80)) {
	
		fileoffset++;

		if (htdata[fileoffset] == vstr[vno]) vno++;
		else vno = 0;
		
		if (vno == 5) foundPrgmHeader = true;
		
	}
	
	if (!foundPrgmHeader) return 0xffff;
	fileoffset++;
	return fileoffset;
	
}



//determine savestate LUT offset
int mainFrame::getLUToffset(char statev, wxFileOffset filesize) {

	bool foundLUT = false;
	int fileoffset = 0;
	int vno = 0;
	
	if (statev > 0) {			//for savestate version 1+, detect "XSAVE" string
		const char vstr[5] = { 0x58, 0x53, 0x41, 0x56, 0x45 };
	
		while ((!foundLUT) && (fileoffset < static_cast<int>(filesize))) {
			fileoffset++;
		
			if (htdata[fileoffset] == vstr[vno]) vno++;
			else vno = 0;
		
			if (vno == 5) foundLUT = true;

		}
	} else {				//for legacy savestates, use slightly unsafe detection via the kick drum sample location
		const char vstr[49] = { 0x70, 0x70, 0x60, 0x60, 0x50, 0x50, 0x40, 0x40, 0x40, 0x30, 0x30, 0x30, 0x30,
					0x20, 0x20, 0x20, 0x20, 0x20, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8, 
					0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x4, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x2, 0x0 };
					
		while ((!foundLUT) && (fileoffset < static_cast<int>(filesize))) {
			fileoffset++;
		
			if (htdata[fileoffset] == vstr[vno]) vno++;
			else vno = 0;
		
			if (vno == 49) {
				foundLUT = true;
				fileoffset+= 5125;	
			}
		}
	}	
	
	if (!foundLUT) fileoffset = -1;
	
	return fileoffset;
}

//read savestate LUT and construct a listing from it
void mainFrame::readLUT(int fileoffset) {

	unsigned stateEnd;
	wxString temp;

	for (int i=0; i<8; i++) {
	
		statebeg[i] = 256*htdata[fileoffset+1] + htdata[fileoffset];
		statelen[i] = 256*htdata[fileoffset+3] + htdata[fileoffset+2] -statebeg[i];
		fileoffset += 4;
		stateEnd = statebeg[i] + statelen[i];
		totalSsize += statelen[i];			//also get total size of all savestates
		
		if (statelen[i] != 0) {
			temp = wxString::Format(wxT("%i"),statebeg[i]);
			savestateList->SetItem(i, 1, temp);
			temp = wxString::Format(wxT("%i"),stateEnd);
			savestateList->SetItem(i, 2, temp);
			temp = wxString::Format(wxT("%i"),statelen[i]);
			savestateList->SetItem(i, 3, temp);
		} else {
			temp = "-----";
			savestateList->SetItem(i, 1, temp);
			savestateList->SetItem(i, 2, temp);
			savestateList->SetItem(i, 3, temp);
		}
		
	}
	
	for (int j=0; j<4; j++) {
	
		savestateList->SetColumnWidth(j,wxLIST_AUTOSIZE);
		
	}
		
	return;	
}



bool mainFrame::exportState(wxString currentStateDoc, wxInt16 itemnr) {

	wxUint8 *sdata;
	sdata = NULL;
	wxFile statefile;
	
	
	if (!statefile.Open(currentStateDoc, wxFile::write)) {
	
		wxMessageDialog error1(NULL, wxT("Error: Could not save file."), wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		error1.ShowModal();
		return false;
		
	}
	
	int fileoffset = statebeg[itemnr] - baseDiff;
	
	sdata = new wxUint8[statelen[itemnr]+2];
	
	sdata[0] = static_cast<unsigned char>(statever);
	sdata[1] = static_cast<unsigned char>(htver);
	
	for (int j=2; j<int(statelen[itemnr]+2); j++) {
	
		sdata[j] = htdata[fileoffset];
		fileoffset++;
		
	}
	
	statefile.Write("HT2SAVE", 7);
	statefile.Write(sdata, (size_t) statelen[itemnr]+2);
	
	statefile.Close();
	delete [] sdata;
	sdata = NULL;
	
	return true;

}



bool mainFrame::insertState(wxString currentStateDoc) {

	//open state file and perform validity checks
	wxFile ht2s(currentStateDoc);
	if (!ht2s.IsOpened()) {
		wxMessageDialog error1(NULL, wxT("Error: File could not be opened."), wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		error1.ShowModal();
		return false;
	}
	
	stateSize = ht2s.Length();
	if (stateSize == wxInvalidOffset) {
		wxMessageDialog error2(NULL, wxT("Error: File is corrupt."), wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		error2.ShowModal();
		return false;
	}
	
	//read in data and perform more validity checks
	stateData = new wxUint8[stateSize];
	
	if (ht2s.Read(stateData, (size_t) stateSize) != stateSize) {
		wxMessageDialog error3(NULL, wxT("Error: File could not be read."), wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		error3.ShowModal();
	
		delete[] stateData;
		stateData = NULL;
		return false;
	}

	ht2s.Close();
	
	//check if we've got an actual ht2s file
	wxString sHeader = "";
	for (int i=0; i<7; i++) {
		sHeader += stateData[i];
	}
	if (sHeader != "HT2SAVE") {
		wxMessageDialog error4(NULL, wxT("Error: Not a valid HT2 savestate."), wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		error4.ShowModal();
	
		delete[] stateData;
		return false;		
	}
	
	//check version of the ht2s file against savestate version of the ht2 executable
	if (stateData[7] > statever) {
	
		wxMessageDialog warn2(NULL, wxT("Error: The savestate you are trying to insert is not supported by this version of HT2."),
		   wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		warn2.ShowModal();
		
		delete[] stateData;
		stateData = NULL;
		return false;
			
	}

	if (stateSize - 9 > getFreeMem()) {
	
		wxMessageDialog error5(NULL, wxT("Error: Not enough space to insert savestate.\nTry deleting something first."),
		   wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		error5.ShowModal();
		
		delete[] stateData;
		stateData = NULL;
		return false;
	}	
	
	//check version of the ht2s file again HT2 version
	if (stateData[8] < htver) {

		wxMessageDialog *askUpgrade = new wxMessageDialog(NULL, wxT("The savestate was extracted from an older version of HT2 than "
		   "the one you're currently using.\n\nAttempt to automatically upgrade it?\n\n"
		   "Note: This may not work correctly if you're using a beta version of HT2."), 
		   wxT("Upgrade state?"), wxYES_NO | wxYES_DEFAULT | wxICON_QUESTION);
		
		if (askUpgrade->ShowModal() == wxID_YES) {
		
			askUpgrade->Destroy();
			if (!upgradeState()) return false;
		
		} else askUpgrade->Destroy();
		
		
	}
	
	if (stateData[8] > htver) {
	
		wxMessageDialog warn1(NULL, wxT("Warning: This savestate was extracted from a newer version of HT2 than "
		   "the one you're currently using.\nSome settings and effect commands may not work as intended."),
		   wxT("Warning"), wxOK_DEFAULT|wxICON_WARNING);
 		warn1.ShowModal();
	
	}
	
	//get first free mem address
	unsigned firstFree = getFirstFree();
	
	//get first available slot
	int stateno = 0;
	
	while (statelen[stateno] != 0) {
	
		stateno++;
		
	}
	
	//insert savestate
	int writeOffset = firstFree - baseDiff;
	
	for (int i=9; i<stateSize; i++) {
	
		htdata[writeOffset] = stateData[i];
		writeOffset++;
		
	}
	
	//rewrite savestate LUT
	writeOffset = lutOffset + (stateno * 4);
	htdata[writeOffset] = (unsigned char)(firstFree & 0xff);
	htdata[writeOffset+1] = (unsigned char)((firstFree/256) & 0xff);
	htdata[writeOffset+2] = (unsigned char)((firstFree+stateSize-9) & 0xff);
	htdata[writeOffset+3] = (unsigned char)(((firstFree+stateSize-9)/256) & 0xff);
	
	writeChecksum();
	
	readLUT(lutOffset);
	
	wxString freeMem = wxString::Format("%i", getFreeMem());
	htSizeInfo->SetLabel("free mem: " + freeMem + " bytes");		

	unsavedChanges = true;
	wxTopLevelWindow::SetTitle("ht2util - " + CurrentDocPath + " [modified]");
	
	delete[] stateData;
	stateData = NULL;
	return true;

}



bool mainFrame::exportAsm(wxString currentAsmDoc, long i) {

	wxFile asmFile;
	if (!asmFile.Open(currentAsmDoc, wxFile::write)) {
	
		wxMessageDialog error1(NULL, wxT("Error: Could not save file."), wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		error1.ShowModal();
		
		return false;
	}
	
	wxFileOffset fileoffset = statebeg[i] - baseDiff;
	
	asmFile.Write(";HT version 2." + wxString::Format("%i", htver) + "\n;savestate version " + wxString::Format("%d", statever));
	asmFile.Write("\n\nspeed\n\tdb #" + wxString::Format("%x", htdata[fileoffset]));
	asmFile.Write("\n\nusrDrum\n\tdw #" + wxString::Format("%x", htdata[fileoffset+2]) + wxString::Format("%x", htdata[fileoffset+1]));
	asmFile.Write("\n\nlooprow\n\tdb #" + wxString::Format("%x", htdata[fileoffset+3]));
	asmFile.Write("\n\nptns");
	
	fileoffset += 4;


 	//decrunch pattern sequence
 	wxUint16 l = 0;

	do {
	
		if (l > statelen[i]) {			//trap broken savestates so we don't accidentally loop forever
		
			wxMessageDialog error2(NULL, wxT("Error: Savestate is corrupt."), wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
			error2.ShowModal();
			asmFile.Close();
			
			return false;
		}
		
		if (!(l & 3)) asmFile.Write("\n\tdb ");		//create a new line every 4 bytes
		asmFile.Write("#" + wxString::Format("%x", htdata[fileoffset]));
		if (((l & 3) != 3) && (htdata[fileoffset] != 0xff)) asmFile.Write(", ");
		l++;
		fileoffset++;
		
	} while (htdata[fileoffset] != 0xff);

	if (l != 1025) asmFile.Write("\n\tds " + wxString::Format("%i", 1025-l) + ",#ff");		//fill bytes not stored in compressed state

 	//decrunch patterns
 	asmFile.Write("\n\nptn00");
 	wxUint16 j,k;
 	l = 0;
	
	do {

		if ((!(l & 15)) && (htdata[fileoffset] < 0xe0)) asmFile.Write("\n\tdb ");		//create a new line every 16 bytes
		
		if (htdata[fileoffset] < 0xd0) {
		
			asmFile.Write("#" + wxString::Format("%x", htdata[fileoffset]));
			if (((l & 15) != 15) && (htdata[fileoffset] != 0xff)) asmFile.Write(", ");
			l++;

		}
		
		if ((htdata[fileoffset] >= 0xd0) && (htdata[fileoffset] < 0xe0)) {
		
			j = htdata[fileoffset] - 0xcf;
			
			for (k = 0; k < j; k++) {
			
				l++;
				asmFile.Write("#0");
				if (((l & 15) != 15) && (htdata[fileoffset] != 0xff)) asmFile.Write(", ");
				
			}
				
		}
		
		if ((htdata[fileoffset] >= 0xe0) && (htdata[fileoffset] < 0xff)) {
		
			j = (htdata[fileoffset] - 0xdf) * 16;
			asmFile.Write("\n\tds " + wxString::Format("%i", j) + "\n\t");
			l += j;
			
		}

		fileoffset++;

	} while (htdata[fileoffset] != 0xff);		//TODO: checking fileoffset-1 in original ht2util, verify that this actually works!


	fileoffset++;
	l++;
	if (2048 - l != 0) asmFile.Write("\n\tds " + wxString::Format("%i", 2049 - l) + "\n\n");

 	//decrunch fx patterns
 	asmFile.Write("fxptn00\n");

	wxUint8 ctrlb = htdata[fileoffset];
 	l = 0;

 	if (ctrlb < 0xff) {
	
 		do {

			ctrlb = htdata[fileoffset];
 			fileoffset++;

			if (ctrlb == l) {

				asmFile.Write("fxptn" + wxString::Format("%x", ctrlb) + "\tdb ");
				
				for (j = 0; j < 32; j++) {

					asmFile.Write("#" + wxString::Format("%x", htdata[fileoffset]));
					fileoffset++;
					if (j != 31) asmFile.Write(",");
				}
				
				l++;
				
			} else {
			
				for (; (ctrlb & 0x3f) > l; l++) {

					asmFile.Write("fxptn" + wxString::Format("%x", l) + "\tds 32\n");
					
				}

				asmFile.Write("fxptn" + wxString::Format("%x", ctrlb & 0x3f) + "\tdb ");
				
				for (j = 0; j < 32; j++) {

					asmFile.Write("#" + wxString::Format("%x", htdata[fileoffset]));
					fileoffset++;
					if (j != 31) asmFile.Write(",");
					
				}
				
				l++;
				
			}

 		} while (ctrlb < 0x40);
		
	} else asmFile.Write("\tds 2048");		//insert 2048 zerobytes if no fx patterns are found


	asmFile.Close();
	
	return true;

}



bool mainFrame::deleteState(long i) {


	//calculate new savestate lookup table
	wxInt16 j = 0;
	wxUint16 limit = statebeg[i] + statelen[i];
	wxUint16 newLUT[16];

	while (j <= 14) {

		if (static_cast<wxInt16>(j/2) == i) {
		
			newLUT[j] = 0;
			newLUT[j+1] = 0;
			
		} else {
		
			if (statebeg[static_cast<wxInt16>(j/2)] > limit) {
			
				newLUT[j] = statebeg[static_cast<wxInt16>(j/2)] - statelen[i] - 1;
				newLUT[j+1] = statebeg[static_cast<wxInt16>(j/2)] + statelen[static_cast<wxInt16>(j/2)] - statelen[i] - 1;
				
			} else {
			
				newLUT[j] = statebeg[static_cast<wxInt16>(j/2)];
				newLUT[j+1] = statebeg[static_cast<wxInt16>(j/2)] + statelen[static_cast<wxInt16>(j/2)];
				
			}
			
		}
		
		j += 2;
		
	}
	
	//buffer those savestates that need to be moved
	wxInt16 fileoffset = statebeg[i] + statelen[i] - baseDiff + 1;
	wxInt16 statesize = htsize - fileoffset;

	wxUint8 buffer[statesize];

	for (j = 0; j < statesize; j++) {
	
		buffer[j] = htdata[fileoffset];
		fileoffset++;
		
	}

	//write new savestate lookup table
	fileoffset = lutOffset;

	for (j = 0; j < 8; j++) {

		htdata[fileoffset] = static_cast<wxUint8>(newLUT[j*2] & 0xff);
		htdata[fileoffset+1] = static_cast<wxUint8>((newLUT[j*2]/256) & 0xff);
		htdata[fileoffset+2] = static_cast<wxUint8>(newLUT[(j*2)+1] & 0xff);
		htdata[fileoffset+3] = static_cast<wxUint8>((newLUT[(j*2)+1]/256) & 0xff);		
		fileoffset += 4;
			
	}
		
	//move data after the savestate to be deleted down in memory, replace remaining mem with zeroes	
	fileoffset = statebeg[i] - baseDiff;
	wxInt16 length;

	if (!htdata[htsize-3]) length = statesize - 6;
	else length = statesize - 4;

	for (j = 0; j < length; j++) {
	
		htdata[fileoffset] = buffer[j];
		fileoffset++;
		
	}

	//fill rest of savestate section with nullbytes
	for (j = 0; j < static_cast<wxInt16>(statelen[i]); j++) {
	
		htdata[fileoffset] = 0;
		fileoffset++;
		
	}
	
	readLUT(lutOffset);
	
	return true;

}


bool mainFrame::upgradeState() {

	wxInt16 fxBlockStart = 12;
	wxUint8 stopByteCount = 0;
	
	//search for fx block start
	for (; fxBlockStart < stateSize; fxBlockStart++) {
	
		if (stateData[fxBlockStart] == 0xff) stopByteCount++;
		if (stopByteCount == 2) break;
	
	}
	
	if (stopByteCount != 2 || fxBlockStart+1 == stateSize) {
	
		wxMessageDialog error1(NULL, wxT("Error: Savestate is corrupt."),
		   wxT("Error"), wxOK_DEFAULT|wxICON_ERROR);
		error1.ShowModal();
			
		return false;
	
	}
	
	fxBlockStart++;
	if (stateData[fxBlockStart] == 0xff) return true;		//no fx patterns present
	
	wxInt16 fp = fxBlockStart;
	
	//determine number of fx patterns
	wxInt16 fxPtnAmount = static_cast<wxInt16>((stateSize - fxBlockStart - 1) / 33);
	
	//search for fx patterns that are used as custom drum data
	bool usrDrum = false;
	wxInt16 drumDataStart = stateSize;
	wxInt16 drumDataEnd = 0;
	
	//check if custom user drum (Ex) is in use
	for (int i = 0; i < fxPtnAmount; i++) {			
	
		fp++;
			
		for (int j = 0; j < 32; j+=2) {
			
			if ((stateData[fp] & 0xe0) == 0xe0) usrDrum = true;	
			fp += 2;
			
		}
	
	}
	
	//get length of user drum data
	if (usrDrum) {						

		fp = fxBlockStart;
		
		for (int i = 0; i < fxPtnAmount; i++) {
		
			if ((stateData[fp] & 0x7f) == 0x20) {
			
				drumDataStart = fp;
				
				for (; fp < stateSize; fp++) {
				
					if (!stateData[fp]) break;
				
				}
				
				drumDataEnd = fp;
				break;
				
			}
			
			fp += 33;
		
		}
	
	}
	

	bool unsupportedSpeed = false;
	bool usedDxx = false;
	bool ambigiousTh = false;
	
	if (stateData[9] > 0x3f && htver < 12) unsupportedSpeed = true;	
	
	fp = fxBlockStart;
	
	for (int i = 0; i < fxPtnAmount; i++) {
	
		fp++;
		
		if (fp < drumDataStart || fp > drumDataEnd) {
		
			if (stateData[8] < 10 && htver >= 10) {
		
			
				for (int j = 0; j < 32; j+=2) {
			
					if ((stateData[fp + j] & 0xf) == 0xd) {					//remove Dxx
					
						//stateData[fp + j + 1] = (stateData[fp + j + 1] & 0xf) + 0x80;
						stateData[fp + j] = stateData[fp + j] & 0xf0;
						usedDxx = true;
				
					}
			
					if ((stateData[fp + j] & 0xf) == 0xc) stateData[fp + j] += 1;		//Cxx -> Dxx
				
					if ((stateData[fp + j] & 0xf) == 0x4) {					//40x -> 58x
				
						stateData[fp + j] += 1;
						stateData[fp + j + 1] = (stateData[fp + j + 1] & 0xf) + 0x80;
				
					}
				
					if ((stateData[fp + j] & 0xf) == 0x2 && !stateData[fp + j + 1]) ambigiousTh = true;
				
					if ((stateData[fp + j] & 0xf) == 0x3 && !stateData[fp + j + 1]) {	//300 -> 9ff
				
						stateData[fp + j] = (stateData[fp + j] & 0xf0) + 9;
						stateData[fp + j + 1] = (stateData[fp + j + 1] & 0xf) + 0x80;
				
					}
				
					if ((stateData[fp + j] & 0xf) == 0x3) stateData[fp + j + 1] = static_cast<wxUint8>(256 - stateData[fp + j + 1]);	
			
				}
		
			}
		
			if (stateData[8] < 12 && htver >= 12) {
		
				//warn about Fxx with xx > 0x3f
				for (int j = 0; j < 32; j+=2) {
			
					if ((stateData[fp + j] & 0xf) == 0xf) {
				
						if (stateData[fp + j + 1] > 0x3f) unsupportedSpeed = true;
				
					}
			
				}
		
			}
		
			if (stateData[8] < 13 && htver >= 13) {
		
				//update Exx
				for (int j = 0; j < 32; j+=2) {
			
					if ((stateData[fp + j] & 0xf) == 0xe) stateData[fp + j + 1] = (stateData[fp + j + 1] & 0xf) + 0x80;
			
				}
		
			}
		
		}
		
		fp += 32;
	
	}
	

	//construct report
	wxString reportMsg = "Savestate upgraded.";
	
	if (unsupportedSpeed || usedDxx || ambigiousTh) {
	
		reportMsg += "\nHowever, the upgrader ran into some problems that it could not resolve:\n\n";
		if (unsupportedSpeed) reportMsg += "- Speeds higher than 0x3f were used. These have a different effect in this HT2 version.\n";
		if (usedDxx) reportMsg += "- Effect Dxx was used. All occurances have been removed because the old Dxx behaviour is no longer supported.\n";
		if (ambigiousTh) reportMsg += "- Effect 200 was used. This is ambigious and was left alone. You might need to update it manually.\n";
		
	}
	
	wxMessageDialog ureport(NULL, reportMsg,
	   wxT("Upgrade Report"), wxOK_DEFAULT | wxICON_INFORMATION);
	ureport.ShowModal();
			

	return true;
}
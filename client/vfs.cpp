/*
Copyright (c) Sam Jansen and Jesse Baker.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

1. Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

3. The end-user documentation included with the redistribution, if
any, must include the following acknowledgment: "This product includes
software developed by Sam Jansen and Jesse Baker 
(see http://www.wand.net.nz/~stj2/bung)."
Alternately, this acknowledgment may appear in the software itself, if
and wherever such third-party acknowledgments normally appear.

4. The hosted project names must not be used to endorse or promote
products derived from this software without prior written
permission. For written permission, please contact sam@wand.net.nz or
jtb5@cs.waikato.ac.nz.

5. Products derived from this software may not use the "Bung" name
nor may "Bung" appear in their names without prior written
permission of Sam Jansen or Jesse Baker.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL COLLABNET OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// $Id$
#include "stdafx.h"
#include "vfs.h"
#include "exception.h"
#include "reporter.h"
#include "misc.h"
//#include <zlib.h>

static CVFS vfs;

void CVFS::LoadArchive(string name) {
	FILE *f = fopen(name.c_str(), "rb");

	if(!f)
		throw CException("Unable to load archive.");

	CFileReader *fr = new CFileReader(f); // NB: this never gets deleted! It really needs fixing at some point
	CArchive arch(fr);

	vfs.archives[name] = arch;
		//.insert( map<string, CArchive>::value_type(name, arch) );
}

CReader *CVFS::LoadFile(string name) {
	return vfs.LoadFileS(name);
}

CReader *CVFS::LoadFileS(string name) {
	string::size_type f = name.find(".barc", 0);
	if(f == string::npos) {
		CFileReader *reader;
		FILE *file = fopen(name.c_str(), "rb");
		
		if(!file) {
			throw CException(string("Unable to open file ") +
				name + string(" for reading."));
		}

		reader = new CFileReader(file); // check for compressed file here in a generic way

		return reader;
	}
	else {
		string::size_type r = name.rfind("/", f), end = f + 5;
		string archiveName, fileName;
		map<string, CArchive>::iterator archiveIter;

		if(r == string::npos)
			r = name.rfind("\\", f);

		if(r == string::npos) {
			archiveName = name.substr(0, end);
			fileName = name.substr(end + 1, name.length() - end);
		}
		else {
			archiveName = name.substr(0, end);
			fileName = name.substr(end + 1, name.length() - end);
		}

		archiveIter = archives.find(archiveName);

		if(archiveIter == archives.end()) {
			throw CException(
				bsprintf("Unknown/unregistered archive given in LoadFile: archive='%s'.", archiveName.c_str())
				);
		}

		return archiveIter->second.LoadFile(fileName);
	}
}
////////////////////////////////////////////////////////////////////////////
// CArchive functions:
CReader *CArchive::LoadFile(string name) {
	map<string, archive_file>::iterator fileIter;

	fileIter = fileEntries.find(name);

	if(fileIter == fileEntries.end()) {
		string::size_type st;
		while( (st = name.find_first_of("\\")) != string::npos) {
			name[st] = '/';
		}
	
		fileIter = fileEntries.find(name);

		if(fileIter == fileEntries.end()) {
			throw CException(string("Bad file in CArchive::LoadFile() - ") + name);
		}
	}

	CReader *reader = new CDummyReader(archiveReader);
	reader = new CArchiveReader(reader, fileIter->second.offset, 
		fileIter->second.com_length == 0 ? fileIter->second.unc_length : 
		fileIter->second.com_length);

	/*if(fileIter->second.com_length != 0) {
		reader = new CGZReader(reader, fileIter->second.unc_length);
	}*/

	return reader;
}

void CArchive::LoadFileEntries()
{
	archiveReader->Seek(0);
	archiveReader->Read(&header, sizeof(archive_header));

	if(strncmp(header.barc, "BARC", 4) != 0)
	{
		string err = "Magic numbers for archive ";
		err += " do not match 'BARC'.";
		throw err;
	}

	if(header.version != 1)
	{
		string err = "Archive version unsupported or bad archive file: ";
		err += " version: ";
		err += (uint32)header.version;
		err += " expected: 1";

		throw err;
	}

	fileEntries.clear();

	for(unsigned int i = 0; i < header.num_files; i++)
	{
		archive_file arc_file;

		archiveReader->Read(&arc_file, sizeof(archive_file));

		fileEntries.insert( map<string, archive_file>::value_type(string(arc_file.name), arc_file));
	}
}

CArchive::~CArchive() { }
////////////////////////////////////////////////////////////////////////////
// File reader functions:
int CFileReader::Read(void *buf, uint32 length) {
	fread(buf, length, 1, file);
	return length;
}

int CFileReader::Seek(uint32 offset) {
	return fseek(file, offset, SEEK_SET);
}

uint32 CFileReader::GetLength() {
	int length, offset;
	offset = ftell(file);
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, offset, SEEK_SET);
	return length;
}
////////////////////////////////////////////////////////////////////////////
// Archive reader functions:
CArchiveReader::CArchiveReader(CReader *archive, int archOffset, int length) {
	archiveOffset = archOffset;
	fileLength = length;
	internalOffset = 0;
	parent = archive;
}

int CArchiveReader::Read(void *buf, uint32 length) {
	int read;
	if(internalOffset >= fileLength) {
		throw CEOFException("CArchiveReader went past EOF in Read().");
	}
	parent->Seek(archiveOffset + internalOffset);
	read = parent->Read(buf, length);
	internalOffset += read;
	return read;
}

int CArchiveReader::Seek(uint32 offset)  {
	if(offset > GetLength()) {
		throw CEOFException("CArchiveReader went past EOF in Seek().");
	}
	internalOffset = offset;
	return offset;
}

uint32 CArchiveReader::GetLength()  {
	return fileLength;
}
////////////////////////////////////////////////////////////////////////////
// Text reader functions:
int CTextReader::ReadLine(char *buf, int maxlen)
{
	char c;
	int count = 0;

	do {
		if(count + 1 >= maxlen) {
			buf[count] = '\0';
			return count;
		}

		try {
			parent->Read(&c, 1);
		} catch(CEOFException &e) {
			return -1; /// hmmmmmm...
		}
		
		if(c == '\r') continue;
		if(c == '\n') { 
			buf[count] = '\0';
			return count;
		}
		
		buf[count] = c;

		count++;
	} while(c != '\0');
	return count;
}
////////////////////////////////////////////////////////////////////////////

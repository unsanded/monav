/*
Copyright 2011  Christian Vetter veaac.fdirct@gmail.com

This file is part of MoNav.

MoNav is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

MoNav is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with MoNav.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SRTMDATA_H
#define SRTMDATA_H

#include <QString>

class SRTMData
{

public:

	static const int SRTM_DATA_VOID = -32768;

	SRTMData();
	~SRTMData();

	// sets a cache dir for SRTM tile
	// required by getAltitude
	void setCacheDir( QString );
	QString cacheDir();

	// sets the amount of tile kept in memory
	void setMemCacheSize( long long bytes );
	long long memCacheSize();

	double getAltitude( double latitude, double longitude );

private:

	struct PrivateImplementation;
	PrivateImplementation* d;
};

#endif // SRTMDATA_H

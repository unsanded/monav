/*
Copyright 2010  Christian Vetter veaac.fdirct@gmail.com

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

#ifndef CHTSettingsDialog_H
#define CHTSettingsDialog_H

#include <QWidget>

class QSettings;

namespace Ui {
	 class CHTSettingsDialog;
}

class CHTSettingsDialog : public QWidget {

	 Q_OBJECT

public:

	 CHTSettingsDialog( QWidget *parent = 0 );
	 ~CHTSettingsDialog();

	struct Settings
	{
		int blockSize;
	};

	bool getSettings( Settings* settings );
	bool loadSettings( QSettings* settings );
	bool saveSettings( QSettings* settings );

protected:

	 Ui::CHTSettingsDialog *m_ui;
};

#endif // CHTSettingsDialog_H

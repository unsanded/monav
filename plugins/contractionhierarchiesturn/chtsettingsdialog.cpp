#include "chtsettingsdialog.h"
#include "ui_chtsettingsdialog.h"
#include <QSettings>
#include <omp.h>

CHTSettingsDialog::CHTSettingsDialog(QWidget *parent) :
	 QWidget(parent),
	 m_ui(new Ui::CHTSettingsDialog)
{
	 m_ui->setupUi(this);
}

CHTSettingsDialog::~CHTSettingsDialog()
{
	 delete m_ui;
}

bool CHTSettingsDialog::getSettings( Settings* settings )
{
	if ( settings == NULL )
		return false;
	settings->blockSize = m_ui->blockSize->value();
	return true;
}

bool CHTSettingsDialog::loadSettings( QSettings* settings )
{
	settings->beginGroup( "Contraction Hierarchies Turn" );
	m_ui->blockSize->setValue( settings->value( "blockSize", 12 ).toInt() );
	settings->endGroup();
	return true;
}

bool CHTSettingsDialog::saveSettings( QSettings* settings )
{
	settings->beginGroup( "Contraction Hierarchies Turn" );
	settings->setValue( "blockSize", m_ui->blockSize->value() );
	settings->endGroup();
	return true;
}

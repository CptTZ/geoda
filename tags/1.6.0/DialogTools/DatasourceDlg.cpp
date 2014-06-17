/**
 * GeoDa TM, Copyright (C) 2011-2014 by Luc Anselin - all rights reserved
 *
 * This file is part of GeoDa.
 * 
 * GeoDa is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * GeoDa is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <vector>
#include <string>
#include <fstream>
#include <wx/progdlg.h>
#include <wx/filedlg.h>
#include <wx/filefn.h> 
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/checkbox.h>
#include <wx/xrc/xmlres.h>
#include <wx/regex.h>
#include "../Project.h"
#include "../DataViewer/DataSource.h"
#include "../DataViewer/DbfTable.h"
#include "../DataViewer/TableInterface.h"
#include "../GenUtils.h"
#include "../logger.h"
#include "../ShapeOperations/OGRDataAdapter.h"
#include "../GdaException.h"
#include "../GeneralWxUtils.h"
#include "DatasourceDlg.h"

using namespace std;

void DatasourceDlg::Init()
{
    m_ds_menu = NULL;
	ds_file_path = wxFileName("");
    
    // create file type dataset pop-up menu dynamically
	ds_names.Add("ESRI Shapefiles (*.shp)|*.shp");
    ds_names.Add("Comma Separated Value (*.csv)|*.csv");
    ds_names.Add("dBase database file (*.dbf)|*.dbf");
    if( GeneralWxUtils::isWindows()){
        ds_names.Add("ESRI Personal Geodatabase (*.mdb)|*.mdb");
    }
    ds_names.Add("ESRI File Geodatabase (*.gdb)|*.gdb");
    ds_names.Add("GeoJSON (*.geojson;*.json)|*.geojson;*.json|"
                 "GeoJSON (*.geojson)|*.geojson|"
                 "GeoJSON (*.json)|*.json");
    ds_names.Add("GML (*.gml)|*.gml");
    ds_names.Add("KML (*.kml)|*.kml");
    ds_names.Add("MapInfo (*.tab;*.mif;*.mid)|*.tab;*.mif;*.mid|"
                 "MapInfo Tab (*.tab)|*.tab|"
                 "MapInfo MID (*.mid)|*.mid|"
                 "MapInfo MID (*.mif)|*.mif");
    ds_names.Add("MS Excel (*.xls)|*.xls");
    //ds_names.Add("Idrisi Vector (*.vct)|*.vct");
    //ds_names.Add("MS Office Open XML Spreadsheet (*.xlsx)|*.xlsx");
    //ds_names.Add("OpenStreetMap XML and PBF (*.osm)|*.OSM;*.osm");
    ds_names.Add("SQLite/SpatiaLite (*.sqlite)|*.sqlite");
    //XXX: looks like tiger data are downloaded as Shapefile, geodatabase etc.
    //ds_names.Add("U.S. Census TIGER/Line (*.tiger)|*.tiger");
    
    // create database tab drop-down list items dynamically
    DBTYPE_ORACLE = "Oracle Spatial Database";
    if( GeneralWxUtils::isX64() ) DBTYPE_ARCSDE = "ESRI ArcSDE (ver 10.x)";
    else if ( GeneralWxUtils::isX86() ) DBTYPE_ARCSDE = "ESRI ArcSDE (ver 9.x)";
    DBTYPE_POSTGIS = "PostgreSQL/PostGIS Database";
    DBTYPE_MYSQL = "MySQL Spatial Database";
}

void DatasourceDlg::CreateControls()
{
    m_ds_filepath_txt = XRCCTRL(*this, "IDC_FIELD_ASC",wxTextCtrl);
	m_database_type = XRCCTRL(*this, "IDC_CDS_DB_TYPE",wxChoice);
	m_database_name = XRCCTRL(*this, "IDC_CDS_DB_NAME",AutoTextCtrl);
	m_database_host = XRCCTRL(*this, "IDC_CDS_DB_HOST",AutoTextCtrl);
	m_database_port = XRCCTRL(*this, "IDC_CDS_DB_PORT",AutoTextCtrl);
	m_database_uname = XRCCTRL(*this, "IDC_CDS_DB_UNAME",AutoTextCtrl);
	m_database_upwd = XRCCTRL(*this, "IDC_CDS_DB_UPWD",wxTextCtrl);
	//m_database_table = XRCCTRL(*this, "IDC_CDS_DB_TABLE",AutoTextCtrl);
	m_ds_notebook = XRCCTRL(*this, "IDC_DS_NOTEBOOK", wxNotebook);
	m_ds_browse_file_btn = XRCCTRL(*this, "IDC_OPEN_IASC",wxBitmapButton);
    
    m_database_type->Append(DBTYPE_ORACLE);
    if(GeneralWxUtils::isWindows()) m_database_type->Append(DBTYPE_ARCSDE);
    m_database_type->Append(DBTYPE_POSTGIS);
    m_database_type->Append(DBTYPE_MYSQL);
    m_database_type->SetSelection(0);
    
    // for autocompletion of input boxes in Database Tab
	std::vector<std::string> host_cands =
        OGRDataAdapter::GetInstance().GetHistory("db_host");
	std::vector<std::string> port_cands =
        OGRDataAdapter::GetInstance().GetHistory("db_port");
	std::vector<std::string> uname_cands =
        OGRDataAdapter::GetInstance().GetHistory("db_user");
	std::vector<std::string> name_cands =
        OGRDataAdapter::GetInstance().GetHistory("db_name");

	m_database_host->SetAutoList(host_cands);
	m_database_port->SetAutoList(port_cands);
	m_database_uname->SetAutoList(uname_cands);
	m_database_name->SetAutoList(name_cands);
}

/**
 * Prompt layer names of input datasource to user for selection.
 * This function is used by OnLookupDSTableBtn, OnOkClick
 */
void DatasourceDlg::PromptDSLayers(IDataSource* datasource)
{
	wxString ds_name = datasource->GetOGRConnectStr();

	if (ds_name.IsEmpty()) {
        wxString msg = "Can't get layers from unknown datasource. "
        "Please complete the datasource fields.";
		throw GdaException(msg.mb_str());
	}
	vector<string> table_names = OGRDataAdapter::GetInstance()
					.GetLayerNames(ds_name.ToStdString());
    int n_tables = table_names.size();
	if ( n_tables > 0 ) {
		wxString *choices = new wxString[n_tables];
		for	(int i=0; i<n_tables; i++) choices[i] = table_names[i];
		wxSingleChoiceDialog choiceDlg(NULL,
                                    "Please select the layer name to connect:",
                                    "Layer names",
                                    n_tables, choices);
		if (choiceDlg.ShowModal() == wxID_OK) {
			if (choiceDlg.GetSelection() >= 0) {
				layer_name = choiceDlg.GetStringSelection();
			}
		}
		delete[] choices;
	} else if ( n_tables == 0) {
		wxMessageDialog dlg(NULL, "There is no layer found in current "
			"data source.", "Info", wxOK | wxICON_INFORMATION);
		dlg.ShowModal();
	} else {
        wxString msg = "No layer has been selected. Please select a layer.";
		throw GdaException(msg.mb_str());
	}
}

/**
 * This function handles the event of user click the open button in File Tab
 * When click browse datasource file button
 */
void DatasourceDlg::OnBrowseDSfileBtn ( wxCommandEvent& event )
{
	// create pop-up datasource menu dynamically
    if ( m_ds_menu == NULL ){
        m_ds_menu = new wxMenu;
        for ( size_t i=0; i < ds_names.GetCount(); i++ ) {
            m_ds_menu->Append( ID_DS_START + i, ds_names[i].BeforeFirst('|'));
        }
    }
    this->PopupMenu(m_ds_menu);
}

/**
 * This function handles the event of user click the pop-up menu of file type
 * data sources.
 * For some menu options (e.g. ESR File GDB, or ODBC based mdb), a directory
 * selector or input dialog need to be prompted to users for further information
 * Otherwise, prompt a file selector to user.
 */
void DatasourceDlg::BrowseDataSource( wxCommandEvent& event)
{
    
	int index = event.GetId() - ID_DS_START;
    wxString name = ds_names[index];
    
    if (name.Contains("gdb")) {
        // directory data source, such as ESRI .gdb directory
        wxDirDialog dlg(this, "Choose a spatial diretory to open","");
        if (dlg.ShowModal() != wxID_OK) return;
        ds_file_path = dlg.GetPath();
        m_ds_filepath_txt->SetValue(ds_file_path.GetFullPath());
        FindWindow(XRCID("wxID_OK"))->Enable(true);
        
    }
#ifdef _WIN64 || __amd64__
    else if (name.Contains("mdb")){
        // 64bit Windows only accept DSN ODBC for ESRI PGeo
        wxTextEntryDialog dlg(this, "Input the DSN name of ESRI Personal "
                              "Geodatabase (.mdb) that is created in ODBC Data "
                              "Source Administrator dialog box.", "DSN name:",
                              "");
        if (dlg.ShowModal() != wxID_OK) return;
        ds_file_path = dlg.GetValue().Trim();
        m_ds_filepath_txt->SetValue( "PGeo:"+dlg.GetValue() );
    }
#endif
    else {
        // file data source
        wxString wildcard;
        wildcard << name;
        wxFileDialog dlg(this,"Choose a spatial file to open", "","",wildcard);
        if (dlg.ShowModal() != wxID_OK) return;
        
        ds_file_path = dlg.GetPath();
        m_ds_filepath_txt->SetValue(ds_file_path.GetFullPath());
        FindWindow(XRCID("wxID_OK"))->Enable(true);
    }
}

wxString DatasourceDlg::GetProjectTitle()
{
    return wxString(layer_name);
}
wxString DatasourceDlg::GetLayerName()
{
    return wxString(layer_name);
}

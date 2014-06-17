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

#include "../DataViewer/DataSource.h"
#include "../ShapeOperations/OGRDataAdapter.h"
#include "../GenUtils.h"
#include "../logger.h"
#include "../GdaException.h"
#include "../GeneralWxUtils.h"
#include "ConnectDatasourceDlg.h"
#include "DatasourceDlg.h"

BEGIN_EVENT_TABLE( ConnectDatasourceDlg, wxDialog )
    EVT_BUTTON(XRCID("IDC_OPEN_IASC"), ConnectDatasourceDlg::OnBrowseDSfileBtn)
	EVT_BUTTON(XRCID("ID_BTN_LOOKUP_TABLE"),
               ConnectDatasourceDlg::OnLookupDSTableBtn)
	EVT_BUTTON(XRCID("ID_BTN_LOOKUP_WSLAYER"),
               ConnectDatasourceDlg::OnLookupWSLayerBtn)
    EVT_BUTTON(wxID_OK, ConnectDatasourceDlg::OnOkClick )
END_EVENT_TABLE()

using namespace std;

ConnectDatasourceDlg::ConnectDatasourceDlg(wxWindow* parent, const wxPoint& pos,
										   const wxSize& size)
:datasource(NULL)
{
    // init controls defined in parent class
    DatasourceDlg::Init();
    
	SetParent(parent);
	CreateControls();
	SetPosition(pos);
	Centre();
    
    Bind( wxEVT_COMMAND_MENU_SELECTED, &ConnectDatasourceDlg::BrowseDataSource,
         this, DatasourceDlg::ID_DS_START, ID_DS_START + ds_names.Count());
}

ConnectDatasourceDlg::~ConnectDatasourceDlg()
{
    if ( datasource != NULL ) {
        delete datasource;
        datasource = NULL;
    }
}

void ConnectDatasourceDlg::CreateControls()
{
    bool test = wxXmlResource::Get()->LoadDialog(this, GetParent(),
                                                 "IDD_CONNECT_DATASOURCE");
    FindWindow(XRCID("wxID_OK"))->Enable(true);
    // init db_table control that is unique in this class
	m_webservice_url = XRCCTRL(*this, "IDC_CDS_WS_URL",AutoTextCtrl);
	m_database_lookup_table =
        XRCCTRL(*this, "ID_BTN_LOOKUP_TABLE", wxBitmapButton);
	m_database_lookup_wslayer =
        XRCCTRL(*this, "ID_BTN_LOOKUP_WSLAYER", wxBitmapButton);
	m_database_table = XRCCTRL(*this, "IDC_CDS_DB_TABLE", wxTextCtrl);
    // create controls defined in parent class
    DatasourceDlg::CreateControls();
    // setup WSF auto-completion
	std::vector<std::string> ws_url_cands =
        OGRDataAdapter::GetInstance().GetHistory("ws_url");
	m_webservice_url->SetAutoList(ws_url_cands);
}

/**
 * This functions handles the event of user click the "lookup" button in
 * Web Service tab
 */
void ConnectDatasourceDlg::OnLookupWSLayerBtn( wxCommandEvent& event )
{
    //XXX handling invalid wfs url:
    //http://walkableneighborhoods.org/geoserver/wfs
	OnLookupDSTableBtn(event);
}

/**
 * This functions handles the event of user click the "lookup" button in
 * Database Tab.
 * When click "lookup" button in Datasource Table, this will call
 * PromptDSLayers() function
 */
void ConnectDatasourceDlg::OnLookupDSTableBtn( wxCommandEvent& event )
{
	try {
        CreateDataSource();
        PromptDSLayers(datasource);
        m_database_table->SetValue(layer_name );
	} catch (GdaException& e) {
		wxString msg;
		msg << e.what();
		wxMessageDialog dlg(this, msg , "Error", wxOK | wxICON_ERROR);
		dlg.ShowModal();
        if( datasource!= NULL &&
            msg.StartsWith("Failed to open data source") ) {
            if ( datasource->GetType() == GdaConst::ds_oci ){
               wxExecute("open https://geodacenter.asu.edu/geoda/setup-oracle");
            } else if ( datasource->GetType() == GdaConst::ds_esri_arc_sde ){
               wxExecute("open https://geodacenter.asu.edu/geoda/setup-arcsde");
			} else if ( datasource->GetType() == GdaConst::ds_esri_file_geodb ){
				wxExecute("open https://geodacenter.asu.edu/geoda/setup-esri-fgdb");
            } else {
				wxExecute("open https://geodacenter.asu.edu/geoda/formats");
			}
        }
	}
}

/**
 * This function handles the event of user click OK button.
 * When user chooses a data source, validate it first,
 * then create a Project() that will be used by the
 * main program.
 */
void ConnectDatasourceDlg::OnOkClick( wxCommandEvent& event )
{
	LOG_MSG("Entering ConnectDatasourceDlg::OnOkClick");
	try {
		CreateDataSource();
        // Check to make sure to get a layer name
        wxString layername;
		int datasource_type = m_ds_notebook->GetSelection();
		if (datasource_type == 0) {
            // File table is selected
			if (layer_name.IsEmpty()) {
				layername = ds_file_path.GetName();
			}
			else {
                // user may select a layer name from Popup dialog that displays
                // all layer names, see PromptDSLayers()
				layername = layer_name;
			}
		} else if (datasource_type == 1) {
            // Database tab is selected
			layername = m_database_table->GetValue();
		} else if (datasource_type == 2) {
            // Web Service tab is selected
            if (layer_name.IsEmpty()) PromptDSLayers(datasource);
			layername = layer_name;
		} else {
            // Should never be here
			return;
		}
        if (layername.IsEmpty()) {
            wxString msg = "Layer name could not be empty. Please select"
                            " a layer.";
            throw GdaException(msg.mb_str());
        }
		// At this point, there is a valid datasource and layername.
        if (layer_name.IsEmpty()) layer_name = layername;
        EndDialog(wxID_OK);
	} catch (GdaException& e) {
		wxString msg;
		msg << e.what();
		wxMessageDialog dlg(this, msg, "Error", wxOK | wxICON_ERROR);
		dlg.ShowModal();
        EndDialog(wxID_CANCEL);
	} catch (...) {
		wxString msg = "Unknow exception. Please contact GeoDa support.";
		wxMessageDialog dlg(this, msg , "Error", wxOK | wxICON_ERROR);
		dlg.ShowModal();
        EndDialog(wxID_CANCEL);
	}
	LOG_MSG("Exiting ConnectDatasourceDlg::OnOkClick");
}

/**
 * After user click OK, create a data source connection string based on user
 * inputs
 * Throw GdaException()
 */
IDataSource* ConnectDatasourceDlg::CreateDataSource()
{
	if (datasource) {
		delete datasource;
		datasource = NULL;
	}
    
	int datasource_type = m_ds_notebook->GetSelection();
	
	if (datasource_type == 0) {
        // File  tab selected
		wxString fn = ds_file_path.GetFullPath();
		if (fn.IsEmpty()) {
			throw GdaException(
                wxString("Please select a datasource file.").mb_str());
		}
#ifdef _WIN64 || __amd64__
        if (m_ds_filepath_txt->GetValue().StartsWith("PGeo:")) {
			fn = "PGeo:" + fn;
		}
#endif
        datasource = new FileDataSource(fn);
        
		// a special case: sqlite is a file based database, so we need to get
		// avalible layers and prompt for user selecting
        if (datasource->GetType() == GdaConst::ds_sqlite ||
            datasource->GetType() == GdaConst::ds_osm ||
            datasource->GetType() == GdaConst::ds_esri_personal_gdb||
            datasource->GetType() == GdaConst::ds_esri_file_geodb)
        {
            PromptDSLayers(datasource);
            if (layer_name.IsEmpty()) {
                throw GdaException(
                    wxString("Layer name could not be empty. Please select"
                             " a layer.").mb_str());
            }
        }
		
	} else if (datasource_type == 1) {
        // Database tab selected
		//int cur_sel = m_database_type->GetSelection();
        wxString cur_sel = m_database_type->GetStringSelection();
		wxString dbname = m_database_name->GetValue().Trim();
		wxString dbhost = m_database_host->GetValue().Trim();
		wxString dbport = m_database_port->GetValue().Trim();
		wxString dbuser = m_database_uname->GetValue().Trim();
		wxString dbpwd  = m_database_upwd->GetValue().Trim();
        
        wxRegEx regex;
        regex.Compile("[0-9]+");
        if (!regex.Matches( dbport )){
            throw GdaException(
                wxString("Please input a valid database port.").mb_str());
        }
        
        GdaConst::DataSourceType ds_type = GdaConst::ds_unknown;
        if (cur_sel == DBTYPE_ORACLE) ds_type = GdaConst::ds_oci;
        else if (cur_sel == DBTYPE_ARCSDE) ds_type = GdaConst::ds_esri_arc_sde;
        else if (cur_sel == DBTYPE_POSTGIS) ds_type = GdaConst::ds_postgresql;
        else if (cur_sel == DBTYPE_MYSQL) ds_type = GdaConst::ds_mysql;
        //else if (cur_sel == 4) ds_type = GdaConst::ds_ms_sql;
        else {
            wxString msg = "The selected database driver is not supported "
            "on this platform. Please check GeoDa website "
            "for more information about database support "
            " and connection.";
            throw GdaException(msg.mb_str());
        }
        datasource = new DBDataSource(dbname, dbhost, dbport, dbuser, dbpwd,
                                      ds_type);
		// save user inputs to history table
		if (!dbhost.IsEmpty())
			OGRDataAdapter::GetInstance()
            .AddHistory("db_host", dbhost.ToStdString());
		if (!dbname.IsEmpty())
			OGRDataAdapter::GetInstance()
            .AddHistory("db_name", dbname.ToStdString());
		if (!dbport.IsEmpty())
			OGRDataAdapter::GetInstance()
            .AddHistory("db_port", dbport.ToStdString());
		if (!dbuser.IsEmpty())
			OGRDataAdapter::GetInstance()
            .AddHistory("db_user", dbuser.ToStdString());
        
		// check if empty, prompt user to input
		wxString error_msg;
		if (dbhost.IsEmpty()) error_msg = "Please input database host.";
		else if (dbname.IsEmpty()) error_msg = "Please input database name.";
		else if (dbport.IsEmpty()) error_msg = "Please input database port.";
		else if (dbuser.IsEmpty()) error_msg = "Please input user name.";
		if (!error_msg.IsEmpty()) {
			throw GdaException(error_msg.mb_str() );
		}
	} else if ( datasource_type == 2 ) {
        // Web Service tab selected
        wxString ws_url = m_webservice_url->GetValue().Trim();
        // detect if it's a valid url string
        wxRegEx regex;
        regex.Compile("^(https|http)://");
        if (!regex.Matches( ws_url )){
            throw GdaException(
                wxString("Please input a valid WFS url address.").mb_str());
        }
        if (ws_url.IsEmpty()) {
            throw GdaException(
                wxString("Please input WFS url.").mb_str());
        } else {
            OGRDataAdapter::GetInstance().AddHistory("ws_url", ws_url.ToStdString());
        }
        if ((!ws_url.StartsWith("WFS:") || !ws_url.StartsWith("wfs:"))
            && !ws_url.EndsWith("SERVICE=WFS")) {
            ws_url = "WFS:" + ws_url;
        }
        datasource = new WebServiceDataSource(ws_url, GdaConst::ds_wfs);
        // prompt user to select a layer from WFS
        //if (layer_name.IsEmpty()) PromptDSLayers(datasource);
    }
	
	return datasource;
}

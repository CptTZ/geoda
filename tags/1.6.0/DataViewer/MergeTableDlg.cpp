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

#include <set>
#include <map>
#include <vector>
#include <wx/xrc/xmlres.h>
#include <wx/msgdlg.h>
#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/textdlg.h>
#include "MergeTableDlg.h"
#include "DataSource.h"
#include "DbfColContainer.h"
#include "TableBase.h"
#include "TableInterface.h"
#include "../ShapeOperations/DbfFile.h"
#include "../ShapeOperations/OGRLayerProxy.h"
#include "../ShapeOperations/OGRDataAdapter.h"
#include "../DialogTools/ConnectDatasourceDlg.h"
#include "../DialogTools/FieldNameCorrectionDlg.h"
#include "../logger.h"

BEGIN_EVENT_TABLE( MergeTableDlg, wxDialog )
	EVT_RADIOBUTTON( XRCID("ID_KEY_VAL_RB"), MergeTableDlg::OnKeyValRB )
	EVT_RADIOBUTTON( XRCID("ID_REC_ORDER_RB"), MergeTableDlg::OnRecOrderRB )
	EVT_BUTTON( XRCID("ID_OPEN_BUTTON"), MergeTableDlg::OnOpenClick )
	EVT_BUTTON( XRCID("ID_INC_ALL_BUTTON"), MergeTableDlg::OnIncAllClick )
	EVT_BUTTON( XRCID("ID_INC_ONE_BUTTON"), MergeTableDlg::OnIncOneClick )
	EVT_LISTBOX_DCLICK( XRCID("ID_INCLUDE_LIST"),
					   MergeTableDlg::OnIncListDClick )
	EVT_BUTTON( XRCID("ID_EXCL_ALL_BUTTON"), MergeTableDlg::OnExclAllClick )
	EVT_BUTTON( XRCID("ID_EXCL_ONE_BUTTON"), MergeTableDlg::OnExclOneClick )
	EVT_LISTBOX_DCLICK( XRCID("ID_EXCLUDE_LIST"),
					   MergeTableDlg::OnExclListDClick )
	EVT_CHOICE( XRCID("ID_CURRENT_KEY_CHOICE"), MergeTableDlg::OnKeyChoice )
	EVT_CHOICE( XRCID("ID_IMPORT_KEY_CHOICE"), MergeTableDlg::OnKeyChoice )
	EVT_BUTTON( XRCID("wxID_OK"), MergeTableDlg::OnMergeClick )
	EVT_BUTTON( XRCID("wxID_CLOSE"), MergeTableDlg::OnCloseClick )
END_EVENT_TABLE()

using namespace std;

MergeTableDlg::MergeTableDlg(TableInterface* _table_int, const wxPoint& pos)
: table_int(_table_int)
{
	SetParent(NULL);
	//table_int->FillColIdMap(col_id_map);
	CreateControls();
	Init();
	wxString nm;
	SetTitle("Merge - " + table_int->GetTableName());
	SetPosition(pos);
    Centre();
}

MergeTableDlg::~MergeTableDlg()
{
    //delete merge_datasource_proxy;
    //merge_datasource_proxy = NULL;
}

void MergeTableDlg::CreateControls()
{
	wxXmlResource::Get()->LoadDialog(this, GetParent(), "ID_MERGE_TABLE_DLG");
	m_input_file_name = wxDynamicCast(FindWindow(XRCID("ID_INPUT_FILE_TEXT")),
									  wxTextCtrl);
	m_key_val_rb = wxDynamicCast(FindWindow(XRCID("ID_KEY_VAL_RB")),
								 wxRadioButton);
	m_rec_order_rb = wxDynamicCast(FindWindow(XRCID("ID_REC_ORDER_RB")),
								   wxRadioButton);
	m_current_key = wxDynamicCast(FindWindow(XRCID("ID_CURRENT_KEY_CHOICE")),
								  wxChoice);
	m_import_key = wxDynamicCast(FindWindow(XRCID("ID_IMPORT_KEY_CHOICE")),
								 wxChoice);
	m_exclude_list = wxDynamicCast(FindWindow(XRCID("ID_EXCLUDE_LIST")),
								   wxListBox);
	m_include_list = wxDynamicCast(FindWindow(XRCID("ID_INCLUDE_LIST")),
								   wxListBox);
}

void MergeTableDlg::Init()
{
	vector<wxString> col_names;
	table_fnames.clear();
	// get the field names from table interface
    set<wxString> field_name_set;
    int time_steps = table_int->GetTimeSteps();
    int n_fields   = table_int->GetNumberCols();
    for (int cid=0; cid<n_fields; cid++) {
        wxString group_name = table_int->GetColName(cid);
        table_fnames.insert(group_name);
        for (int i=0; i<time_steps; i++) {
            GdaConst::FieldType field_type = table_int->GetColType(cid,i);
            wxString field_name = table_int->GetColName(cid, i);
            // only String, Integer can be keys for merging
            if ( field_type == GdaConst::long64_type ||
                field_type == GdaConst::string_type )
            {
                if ( field_name_set.count(field_name) == 0) {
                    m_current_key->Append(field_name);
                    field_name_set.insert(field_name);
                }
            }
            table_fnames.insert(field_name);
        }
    }
	UpdateMergeButton();
}

void MergeTableDlg::OnKeyValRB( wxCommandEvent& ev )
{
	UpdateMergeButton();
}

void MergeTableDlg::OnRecOrderRB( wxCommandEvent& ev )
{
	UpdateMergeButton();
}

void MergeTableDlg::OnOpenClick( wxCommandEvent& ev )
{
    try {
        ConnectDatasourceDlg dlg(this);
        if (dlg.ShowModal() != wxID_OK) return;
        
        wxString proj_title = dlg.GetProjectTitle();
        wxString layer_name = dlg.GetLayerName();
        IDataSource* datasource = dlg.GetDataSource();
        wxString datasource_name = datasource->GetOGRConnectStr();
       
        //XXX: ToStdString() needs to take care of weird file path
        merge_datasource_proxy =
            new OGRDatasourceProxy(datasource_name.ToStdString(), true);
        merge_layer_proxy =
            merge_datasource_proxy->GetLayerProxy(layer_name.ToStdString());
        merge_layer_proxy->ReadData();
        m_input_file_name->SetValue(layer_name);
        
        // get the unique field names, and fill to m_import_key (wxChoice)
        map<wxString, int> dbf_fn_freq;
        dups.clear();
        dedup_to_id.clear();
        for (int i=0, iend=merge_layer_proxy->GetNumFields(); i<iend; i++) {
            GdaConst::FieldType field_type = merge_layer_proxy->GetFieldType(i);
            wxString name = merge_layer_proxy->GetFieldName(i);
            wxString dedup_name = name;
            if (dbf_fn_freq.find(name) != dbf_fn_freq.end()) {
                dedup_name << " (" << dbf_fn_freq[name]++ << ")";
            } else {
                dbf_fn_freq[name] = 1;
            }
            dups.insert(dedup_name);
            dedup_to_id[dedup_name] = i; // map to DBF col id
            if ( field_type == GdaConst::long64_type ||
                 field_type == GdaConst::string_type )
            {
                m_import_key->Append(dedup_name);
            }
            m_exclude_list->Append(dedup_name);
        }
        
    }catch(GdaException& e) {
        wxMessageDialog dlg (this, e.what(), "Error", wxOK | wxICON_ERROR);
		dlg.ShowModal();
        return;
    }
}

void MergeTableDlg::OnIncAllClick( wxCommandEvent& ev)
{
	for (int i=0, iend=m_exclude_list->GetCount(); i<iend; i++) {
		m_include_list->Append(m_exclude_list->GetString(i));
	}
	m_exclude_list->Clear();
	UpdateMergeButton();
}

void MergeTableDlg::OnIncOneClick( wxCommandEvent& ev)
{
	if (m_exclude_list->GetSelection() >= 0) {
		wxString k = m_exclude_list->GetString(m_exclude_list->GetSelection());
		m_include_list->Append(k);
		m_exclude_list->Delete(m_exclude_list->GetSelection());
	}
	UpdateMergeButton();
}

void MergeTableDlg::OnIncListDClick( wxCommandEvent& ev)
{
	OnExclOneClick(ev);
}

void MergeTableDlg::OnExclAllClick( wxCommandEvent& ev)
{
	for (int i=0, iend=m_include_list->GetCount(); i<iend; i++) {
		m_exclude_list->Append(m_include_list->GetString(i));
	}
	m_include_list->Clear();
	UpdateMergeButton();
}

void MergeTableDlg::OnExclOneClick( wxCommandEvent& ev)
{
	if (m_include_list->GetSelection() >= 0) {
		m_exclude_list->
			Append(m_include_list->GetString(m_include_list->GetSelection()));
		m_include_list->Delete(m_include_list->GetSelection());
	}
	UpdateMergeButton();
}

void MergeTableDlg::OnExclListDClick( wxCommandEvent& ev)
{
	OnIncOneClick(ev);
}


void MergeTableDlg::CheckKeys(wxString key_name, vector<wxString>& key_vec,
                              map<wxString, int>& key_map)
{
    for (int i=0, iend=key_vec.size(); i<iend; i++) {
        key_vec[i].Trim(false);
        key_vec[i].Trim(true);
        key_map[key_vec[i]] = i;
    }
	
    if (key_vec.size() != key_map.size()) {
        wxString msg;
        msg << "Chosen table merge key field " << key_name;
        msg << " contains duplicate values. Key fields must contain all ";
        msg << "unique values.";
        throw GdaException(msg.mb_str());
    }
}

vector<wxString> MergeTableDlg::
GetSelectedFieldNames(map<wxString,wxString>& merged_fnames_dict)
{
    vector<wxString> merged_field_names;
    set<wxString> dup_merged_field_names, bad_merged_field_names;

    for (int i=0, iend=m_include_list->GetCount(); i<iend; i++) {
        wxString inc_n = m_include_list->GetString(i);
        merged_field_names.push_back(inc_n);
        if (table_fnames.find(inc_n) != table_fnames.end())
            dup_merged_field_names.insert(inc_n);
        else if (!table_int->IsValidDBColName(inc_n))
            bad_merged_field_names.insert(inc_n);
    }
    
    if ( bad_merged_field_names.size() + dup_merged_field_names.size() > 0) {
        // show a field name correction dialog
        GdaConst::DataSourceType ds_type = table_int->GetDataSourceType();
        FieldNameCorrectionDlg fc_dlg(ds_type, merged_fnames_dict,
                                      merged_field_names,
                                      dup_merged_field_names,
                                      bad_merged_field_names);
        if ( fc_dlg.ShowModal() != wxID_OK ) 
            merged_field_names.clear();
        else
            merged_fnames_dict = fc_dlg.GetMergedFieldNameDict();
    }
    return merged_field_names;
}

void MergeTableDlg::OnMergeClick( wxCommandEvent& ev )
{
    try {
        wxString error_msg;
        
        // get selected field names from merging table
        map<wxString, wxString> merged_fnames_dict;
        for (set<wxString>::iterator it = table_fnames.begin();
             it != table_fnames.end(); ++it ) {
             merged_fnames_dict[ *it ] = *it;
        }
        vector<wxString> merged_field_names =
            GetSelectedFieldNames(merged_fnames_dict);
        
        if (merged_field_names.empty()) return;
        
        int n_rows = table_int->GetNumberRows();
        int n_merge_field = merged_field_names.size();
       
        map<int, int> rowid_map;
        // check merge by key/record order
        if (m_key_val_rb->GetValue()==1) {
            // get and check keys from original table
            int key1_id = m_current_key->GetSelection();
            wxString key1_name = m_current_key->GetString(key1_id);
            int col1_id = table_int->FindColId(key1_name);
            if (table_int->IsColTimeVariant(col1_id)) {
                error_msg = "Chosen key field '";
                error_msg << key1_name << "' is a time variant. Please choose "
                << "a non-time variant field as key.";
                throw GdaException(error_msg.mb_str());
            }
            vector<wxString> key1_vec;
            vector<wxInt64>  key1_l_vec;
            map<wxString,int> key1_map;
            if ( table_int->GetColType(col1_id, 0) == GdaConst::string_type ) {
                table_int->GetColData(col1_id, 0, key1_vec);
            }else if (table_int->GetColType(col1_id,0)==GdaConst::long64_type){
                table_int->GetColData(col1_id, 0, key1_l_vec);
            }
            if (key1_vec.empty()) {
                for( int i=0; i< key1_l_vec.size(); i++){
                    wxString tmp;
                    tmp << key1_l_vec[i];
                    key1_vec.push_back(tmp);
                }
            }
            CheckKeys(key1_name, key1_vec, key1_map);
            
            // get and check keys from import table
            int key2_id = m_import_key->GetSelection();
            wxString key2_name = m_import_key->GetString(key2_id);
            int col2_id = merge_layer_proxy->GetFieldPos(key2_name);
            int n_merge_rows = merge_layer_proxy->GetNumRecords();
            vector<wxString> key2_vec;
            map<wxString,int> key2_map;
            for (int i=0; i < n_merge_rows; i++) {
                key2_vec.push_back(merge_layer_proxy->GetValueAt(i, col2_id));
            }
            CheckKeys(key2_name, key2_vec, key2_map);
            
            // make sure key1 <= key2, and store their mappings
            map<wxString,int>::iterator key1_it, key2_it;
            for (key1_it=key1_map.begin(); key1_it!=key1_map.end(); key1_it++) {
                key2_it = key2_map.find(key1_it->first);
                if ( key2_it == key2_map.end()){
                    error_msg = "The set of values in the import key fields ";
                    error_msg << "do not fully match current table. Please "
                              << "choose keys with matching sets of values.";
                    throw GdaException(error_msg.mb_str());
                }
                rowid_map[key1_it->second] = key2_it->second;
            }
        }
        // merge by order sequence
        else if (m_rec_order_rb->GetValue() == 1) {
            if (table_int->GetNumberRows()>merge_layer_proxy->GetNumRecords()) {
                error_msg = "The number of records in current table is larger ";
                error_msg << "than the number of records in import table. "
                          << "Please choose import table >= "
                          << table_int->GetNumberRows() << "records";
                throw GdaException(error_msg.mb_str());
            }
        }
        // append new fields to original table via TableInterface
        for (int i=0; i<n_merge_field; i++) {
            wxString real_field_name = merged_field_names[i];
            wxString field_name  = real_field_name;
            if (merged_fnames_dict.find(real_field_name) !=
                merged_fnames_dict.end())
            {
                field_name = merged_fnames_dict[real_field_name];
            }
            AppendNewField(field_name, real_field_name, n_rows, rowid_map);
        }
	}
    catch (GdaException& ex) {
        if (ex.type() == GdaException::NORMAL) return;
        wxMessageDialog dlg(this, ex.what(), "Error", wxOK | wxICON_ERROR);
        dlg.ShowModal();
        EndDialog(wxID_CANCEL);
        return;
    }
    
	wxMessageDialog dlg(this, "File merged into Table successfully.",
						"Success", wxOK );
	dlg.ShowModal();
	ev.Skip();
	EndDialog(wxID_OK);	
}

void MergeTableDlg::AppendNewField(wxString field_name,
                                   wxString real_field_name,
                                   int n_rows,
                                   map<int,int>& rowid_map)
{
    int fid = merge_layer_proxy->GetFieldPos(real_field_name);
    GdaConst::FieldType ftype = merge_layer_proxy->GetFieldType(fid);
    if ( ftype == GdaConst::string_type ) {
        int add_pos = table_int->InsertCol(ftype, field_name);
        vector<wxString> data(n_rows);
        for (int i=0; i<n_rows; i++) {
            int import_rid = i;
            if (!rowid_map.empty()) import_rid = rowid_map[i];
            data[i]=wxString(merge_layer_proxy->GetValueAt(import_rid,fid));
        }
        table_int->SetColData(add_pos, 0, data);
    } else if ( ftype == GdaConst::long64_type ) {
        int add_pos = table_int->InsertCol(ftype, field_name);
        vector<wxInt64> data(n_rows);
        for (int i=0; i<n_rows; i++) {
            int import_rid = i;
            if (!rowid_map.empty()) import_rid = rowid_map[i];
            OGRFeature* feat = merge_layer_proxy->GetFeatureAt(import_rid);
            data[i] = feat->GetFieldAsInteger(fid);
        }
        table_int->SetColData(add_pos, 0, data);
    } else if ( ftype == GdaConst::double_type ) {
        int add_pos=table_int->InsertCol(ftype, field_name);
        vector<double> data(n_rows);
        for (int i=0; i<n_rows; i++) {
            int import_rid = i;
            if (!rowid_map.empty()) import_rid = rowid_map[i];
            OGRFeature* feat = merge_layer_proxy->GetFeatureAt(import_rid);
            data[i] = feat->GetFieldAsDouble(fid);
        }
        table_int->SetColData(add_pos, 0, data);
    }
}

void MergeTableDlg::OnCloseClick( wxCommandEvent& ev )
{
	ev.Skip();
	EndDialog(wxID_CLOSE);
}

void MergeTableDlg::OnKeyChoice( wxCommandEvent& ev )
{
	UpdateMergeButton();
}

void MergeTableDlg::UpdateMergeButton()
{
	bool enable = (!m_include_list->IsEmpty() &&
				   (m_rec_order_rb->GetValue()==1 ||
					(m_key_val_rb->GetValue()==1 &&
					 m_current_key->GetSelection() != wxNOT_FOUND &&
					 m_import_key->GetSelection() != wxNOT_FOUND)));
	FindWindow(XRCID("wxID_OK"))->Enable(enable);
}

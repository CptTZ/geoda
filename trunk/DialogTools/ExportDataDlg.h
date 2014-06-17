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
#ifndef __GEODA_CENTER_EXPORT_DATASOURCE_DLG_H__
#define __GEODA_CENTER_EXPORT_DATASOURCE_DLG_H__


#include <vector>
#include <wx/dialog.h>
#include <wx/bmpbuttn.h>
#include <wx/choice.h>
#include <wx/filename.h>
#include <wx/textctrl.h>
#include <wx/sizer.h>
#include <wx/notebook.h>
#include <wx/checkbox.h>

#include "../Project.h"
#include "../ShapeOperations/OGRLayerProxy.h"
#include "AutoCompTextCtrl.h"
#include "DatasourceDlg.h"

class ExportDataDlg: public DatasourceDlg
{
public:
	ExportDataDlg(wxWindow* parent,
				  Project* project,
                  bool isSelectedOnly=false,
                  wxString projectFileName = "",
				  const wxPoint& pos = wxDefaultPosition,
				  const wxSize& size = wxDefaultSize );
	/** NOTE: Project could be NULL in case of creating
	 *  a GRID shape ds or from a BOUNDARY.
	 */
    ExportDataDlg(wxWindow* parent,
                  std::vector<GdaShape*>& _geometries,
                  Shapefile::ShapeType _shape_type,
				  Project* project=NULL,
                  bool isSelectedOnly=false,
                  const wxPoint& pos = wxDefaultPosition,
				  const wxSize& size = wxDefaultSize);
    ExportDataDlg(wxWindow* parent,
                  std::vector<GdaPoint*>& _geometries,
                  Shapefile::ShapeType _shape_type,
				  Project* project=NULL,
                  bool isSelectedOnly=false,
                  const wxPoint& pos = wxDefaultPosition,
				  const wxSize& size = wxDefaultSize);
    void Init(wxWindow* parent, const wxPoint& pos);
    void CreateControls();
    void BrowseExportDataSource( wxCommandEvent& event );
    void OnOkClick( wxCommandEvent& event );

public:
	bool IsTableOnly();
	
private:
	AutoTextCtrl* m_database_table;
	wxCheckBox* m_chk_create_project;
    
	Project* project_p;
	vector<GdaShape*> geometries;
    Shapefile::ShapeType shape_type;
    wxString project_file_name;
	wxFileName ds_file_path;
	wxString ds_format;
	wxString ds_srs;
    bool is_selected_only;
    bool is_create_project;
    bool is_saveas_op;
	// e.g. centroids, grids. vector<GdaShape*> geometries
	// take ownership of external geometries temporarily, 
	// so its memory will be maintained (no cleanup).
	bool is_geometry_only; 
    
	IDataSource* GetDatasource();
    void OpenDatasourceFile(const wxFileName& ds_fname);
    void ExportOGRLayer(wxString& ds_name, bool is_update);
    bool CreateOGRLayer(wxString& ds_name, TableInterface* table,
		OGRSpatialReference* spatial_ref, bool is_update);
    
	DECLARE_EVENT_TABLE()
};

#endif
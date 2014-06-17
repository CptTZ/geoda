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

#include <assert.h>
#include <list>
#include <set>
#include <sstream>
#include <vector>
#include <boost/property_tree/exceptions.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/grid.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/dir.h>
#include "logger.h"
#include "FramesManager.h"
#include "SaveButtonManager.h"
#include "GdaException.h"
#include "DefaultVarsPtree.h"
#include "DataViewer/CustomClassifPtree.h"
#include "DataViewer/OGRTable.h"
#include "DataViewer/DbfTable.h"
#include "DataViewer/TableBase.h"
#include "DataViewer/TableFrame.h"
#include "DataViewer/TableInterface.h"
#include "DataViewer/TableState.h"
#include "DataViewer/TimeState.h"
#include "DataViewer/VarOrderPtree.h"
#include "DialogTools/SaveToTableDlg.h"
#include "DialogTools/ExportDataDlg.h"
#include "Explore/CatClassification.h"
#include "Explore/CatClassifManager.h"
#include "Generic/GdaShape.h"
#include "ShapeOperations/DbfFile.h"
#include "ShapeOperations/GalWeight.h"
#include "ShapeOperations/ShapeUtils.h"
#include "ShapeOperations/shp2cnt.h" // only needed for IsLineShapeFile
#include "ShapeOperations/VoronoiUtils.h"
#include "ShapeOperations/WeightsManager.h"
#include "ShapeOperations/WeightsManPtree.h"
#include "ShapeOperations/OGRDataAdapter.h"
#include "Project.h"

i_array_type Project::shared_category_scratch; // used by TemplateCanvas

/** Constructor for an existing project file */
Project::Project(const wxString& proj_fname)
: is_project_valid(false),
table_int(0), table_state(0), time_state(0), w_manager(0), save_manager(0),
frames_manager(0),cat_classif_manager(0), mean_centers(0), centroids(0),
voronoi_rook_nbr_gal(0), default_var_name(4), default_var_time(4),
point_duplicates_initialized(false), point_dups_warn_prev_displayed(false),
num_records(0), layer_proxy(NULL)
{
	LOG_MSG("Entering Project::Project (existing project)");
	
	SetProjectFullPath(proj_fname);
	bool wd_success = SetWorkingDir(proj_fname);
	if (!wd_success) {
		LOG_MSG("Warning: could not set Working Dir from " + proj_fname);
		// attempt to set working dir according to standard location
		wd_success = SetWorkingDir(wxGetHomeDir());
		if (!wd_success) {
			LOG_MSG("Warning: could not set Working Dir to wxGetHomeDir()");
		}
	}
    project_conf = new ProjectConfiguration(proj_fname);
    LayerConfiguration* layer_conf = project_conf->GetLayerConfiguration();
    layername = layer_conf->GetName();
    datasource = layer_conf->GetDataSource();
	LOG_MSG("Custom Categories:");
	if (layer_conf->GetCustClassifPtree()) {
		LOG_MSG(layer_conf->GetCustClassifPtree()->ToStr());
	}
	LOG_MSG("Weights Manager Meta Info List:");
	if (layer_conf->GetWeightsManPtree()) {
		LOG_MSG(layer_conf->GetWeightsManPtree()->ToStr());
	}
	LOG_MSG("Default Vars List:");
	if (layer_conf->GetDefaultVarsPtree()) {
		LOG_MSG(layer_conf->GetDefaultVarsPtree()->ToStr());
	}
	is_project_valid = CommonProjectInit();
	if (is_project_valid) save_manager->SetAllowEnableSave(true);
	if (is_project_valid) {
		// correct cat classifications in weights manager from Table
		GetCatClassifManager()->VerifyAgainstTable();
	}
	
	LOG_MSG("Exiting Project::Project");
}

/** Constructor for a newly connected datasource */
Project::Project(const wxString& proj_title,
                 const wxString& layername_s,
                 IDataSource* p_datasource)
: is_project_valid(false),
table_int(0), table_state(0), time_state(0), w_manager(0), save_manager(0),
frames_manager(0),cat_classif_manager(0), mean_centers(0), centroids(0),
voronoi_rook_nbr_gal(0), default_var_name(4), default_var_time(4),
point_duplicates_initialized(false), point_dups_warn_prev_displayed(false),
num_records(0), layer_proxy(NULL)
{
	LOG_MSG("Entering Project::Project (new project)");
	
	datasource    = p_datasource->Clone();
	project_title = proj_title;
	layer_title   = layername_s;
	layername     = layername_s;
    
	bool wd_success = false;
	if (FileDataSource* fds = dynamic_cast<FileDataSource*>(datasource)) {
		wxString fp = fds->GetFilePath();
		LOG(fp);
		wd_success = SetWorkingDir(fp);
		if (!wd_success) {
			LOG_MSG("Warning: could not set Working Dir from " + fp);
		}
	}
	if (!wd_success) {
		// attempt to set working dir according to standard location
		wd_success = SetWorkingDir(wxGetHomeDir());
		if (!wd_success) {
			LOG_MSG("Warning: could not set Working Dir to wxGetHomeDir()");
		}
	}
	
    // variable_order instance (table information) is newly created
    // its content will be update in InitFromXXX() by calling function
    // CorrectVarGroups()
    LayerConfiguration* layer_conf = new LayerConfiguration(layername,
                                                            datasource);
    project_conf = new ProjectConfiguration(proj_title, layer_conf);
   
    // Init new project from datasource
	is_project_valid = CommonProjectInit();
	if (is_project_valid) {
		// Project file needs an initial save to existing source can
		// be enabled.
		save_manager->SetAllowEnableSave(true);
        save_manager->SetMetaDataSaveNeeded(true);
	}
	
	LOG_MSG("Exiting Project::Project");
}

Project::~Project()
{
	LOG_MSG("Entering Project::~Project");

    if (project_conf) delete project_conf;
	datasource = 0;
	if (cat_classif_manager) delete cat_classif_manager; cat_classif_manager=0;
	if (w_manager) delete w_manager; w_manager = 0;
	for (size_t i=0, iend=mean_centers.size(); i<iend; i++) delete mean_centers[i];
	for (size_t i=0, iend=centroids.size(); i<iend; i++) delete centroids[i];
	if (voronoi_rook_nbr_gal) delete [] voronoi_rook_nbr_gal;

	OGRDataAdapter::GetInstance().Close();
	
	//NOTE: the wxGrid instance in TableFrame has
	// ownership and is therefore responsible for deleting the
	// table_int when it closes.
	//if (table_int) delete table_int; table_int = 0;

	LOG_MSG("Exiting Project::~Project");
}

GdaConst::DataSourceType Project::GetDatasourceType()
{
    return datasource->GetType();
}

wxString Project::GetProjectFullPath()
{
	LOG_MSG("In Project::GetProjectFullPath");
	wxString fp;
    if (!GetWorkingDir().GetPath().IsEmpty() && !proj_file_no_ext.IsEmpty()) {
		fp << GetWorkingDir().GetPathWithSep();
		fp << proj_file_no_ext << ".gda";
	}
	LOG(fp);
    return fp;
}

void Project::SetProjectFullPath(const wxString& proj_full_path)
{
	LOG_MSG("Entering Project::SetProjectFullPath");
	LOG(proj_full_path);
    wxFileName temp(proj_full_path);
	SetWorkingDir(proj_full_path);
    proj_file_no_ext = temp.GetName();
	LOG_MSG("Exiting Project::SetProjectFullPath");
}

bool Project::SetWorkingDir(const wxString& path)
{
	LOG_MSG("Entering Project::SetWorkingDir");
	LOG(path);
	wxFileName dir;
	if (wxDirExists(path)) {
		dir.AssignDir(path);
	} else {
		dir.Assign(path);
		wxString t = dir.GetPath();
		dir.Clear();
		dir.AssignDir(t);
	}
	if (!dir.IsOk()) {
		LOG_MSG("dir is invalid directory name");
		return false;
	}
	if (!dir.IsDir()) {
		LOG_MSG("dir is not a directory");
		return false;
	}
	if (!dir.DirExists()) {
		LOG_MSG("directory " + dir.GetPath() + " does not exist");
		return false;
	}
	working_dir.Clear();
	working_dir = dir;
	LOG(working_dir.GetPath());
	LOG(working_dir.GetFullPath());
	LOG(working_dir.IsDir());
	LOG_MSG("Exiting Project::SetWorkingDir");
	return true;
}

Shapefile::ShapeType Project::GetGdaGeometries(vector<GdaShape*>& geometries)
{
    Shapefile::ShapeType shape_type = Shapefile::NULL_SHAPE;
    int num_geometries = main_data.records.size();
    if ( main_data.header.shape_type == Shapefile::POINT) {
        Shapefile::PointContents* pc;
        for (int i=0; i<num_geometries; i++) {
            pc = (Shapefile::PointContents*)main_data.records[i].contents_p;
            geometries.push_back(new GdaPoint(wxRealPoint(pc->x, pc->y)));
        }
        shape_type = Shapefile::POINT;
    } else if (main_data.header.shape_type == Shapefile::POLYGON) {
        Shapefile::PolygonContents* pc;
        for (int i=0; i<num_geometries; i++) {
            pc = (Shapefile::PolygonContents*)main_data.records[i].contents_p;
            geometries.push_back(new GdaPolygon(pc));
        }
        shape_type = Shapefile::POLYGON;
    }
    return shape_type;
}

OGRSpatialReference* Project::GetSpatialReference()
{
	 OGRSpatialReference* spatial_ref = NULL;
	 OGRTable* ogr_table = dynamic_cast<OGRTable*>(table_int);
     if (ogr_table != NULL) {
		// it's a OGRTable
        OGRLayerProxy* exist_layer = ogr_table->GetOGRLayer();
        spatial_ref = exist_layer->GetSpatialReference();
        if (spatial_ref) spatial_ref->Clone();
	} else {
        // DbfTable
		wxString ds_name = datasource->GetOGRConnectStr();
        if (!wxFileExists(ds_name)) {
            return NULL;
        }
        OGRDatasourceProxy* ogr_ds =
            new OGRDatasourceProxy(ds_name.ToStdString(), true);
        OGRLayerProxy* ogr_layer =
            ogr_ds->GetLayerProxy(layername.ToStdString());
        spatial_ref = ogr_layer->GetSpatialReference();
        if (spatial_ref) spatial_ref = spatial_ref->Clone();
        delete ogr_ds;
    }
	 return spatial_ref;
}

void Project::SaveOGRDataSource()
{
	// This function will only be called to save file or directory (OGR)
	wxString tmp_prefix = "GdaTmp_";
    wxArrayString all_tmp_files;

    try{        
        bool is_update = false;
        
        // save to a new tmp file(s) to backup original file(s)
        wxString ds_name = datasource->GetOGRConnectStr();
        wxFileName fn(ds_name);
        wxString tmp_ds_name = ds_name;
        if (wxFileExists(ds_name) && fn.GetExt().CmpNoCase("sqlite")!=0) {
            // for existing sqlite file, we add or replace layer
            tmp_ds_name = fn.GetPathWithSep()+tmp_prefix+fn.GetFullName();
            if ( wxFileExists(tmp_ds_name) ) {
                wxRemoveFile(tmp_ds_name);
            }
        }
       
        // special cases that saves by update original datasource
        if ( fn.GetExt().CmpNoCase("sqlite")==0 ) {
            is_update = true;
        }
        
        SaveDataSourceAs(tmp_ds_name, is_update);
        
        // replace old file with tmp files, then delete tmp files
        if (tmp_ds_name!=ds_name && wxFileExists(tmp_ds_name)) {
            wxString tmp_name = fn.GetName();
            wxDir wdir(fn.GetPath());
            wxArrayString tmp_files;
            wdir.GetAllFiles(fn.GetPath(), &tmp_files);
            for (size_t i=0; i < tmp_files.size(); i++) {
                if (tmp_files[i].Contains(tmp_prefix)) {
                    all_tmp_files.push_back(tmp_files[i]);
                }
            }
            for (size_t i=0; i< all_tmp_files.size(); i++) {
                wxString orig_fname = all_tmp_files[i];
                orig_fname.Replace(tmp_prefix, "");
                wxCopyFile(all_tmp_files[i], orig_fname);
                wxRemoveFile(all_tmp_files[i]);
            }
        }
    } catch( GdaException& e){
        for (size_t i=0; i< all_tmp_files.size(); i++) {
            wxRemoveFile(all_tmp_files[i]);
        }
        throw e;
    }
}

void Project::SaveDataSourceAs(const wxString& new_ds_name, bool is_update)
{
    vector<GdaShape*> geometries;
    try {
        // SaveAs only to same datasource
        GdaConst::DataSourceType ds_type = datasource->GetType();
        // SaveAs dbf: using legacy code, not OGR
        if (ds_type == GdaConst::ds_dbf ) {
            wxString save_err_msg;
            DbfTable* dbf_tbl_int = dynamic_cast<DbfTable*>(table_int);
            if ( dbf_tbl_int ) {
                dbf_tbl_int->WriteToDbf(new_ds_name, save_err_msg);
            }
            if ( !save_err_msg.empty() ) {
                throw GdaException(save_err_msg.mb_str());
            }
            return;
        }
        
        if ( wxFileExists(new_ds_name) ) {
            wxRemoveFile(new_ds_name);
        }
        // SaveAs: using OGR
        wxString ds_format = IDataSource::GetDataTypeNameByGdaDSType(ds_type);
        if ( !IDataSource::IsWritable(ds_type) ) {
            std::ostringstream error_message;
            error_message << "GeoDa does not support creating data source "
            << "of " << ds_format << ". Please try to 'Export' to other "
            << "supported data source format.";
            throw GdaException(error_message.str().c_str());
        }
        // call to initial OGR instance
        OGRDataAdapter::GetInstance();
        
		// Get spatial reference from this project
        OGRSpatialReference* spatial_ref = GetSpatialReference();

        // Get Gda geometries and convert to OGR geometries from this project
        Shapefile::ShapeType shape_type = GetGdaGeometries(geometries);
        
        // Get default selected rows: all records should be saved or saveas
		vector<int> selected_rows;
        for (size_t i=0; i<table_int->GetNumberRows(); i++) {
            selected_rows.push_back(i);
        }
        
        vector<OGRGeometry*> ogr_geometries;
		OGRwkbGeometryType geom_type =
			OGRDataAdapter::GetInstance().MakeOGRGeometries(
				geometries, shape_type, ogr_geometries, selected_rows);
       
        // Start saving
        int prog_n_max = 0;
        if (table_int) prog_n_max = table_int->GetNumberRows();
        wxProgressDialog prog_dlg("Save data source progress dialog",
                                  "Saving data...",
                                  prog_n_max, NULL,
                                  wxPD_CAN_ABORT|wxPD_AUTO_HIDE|wxPD_APP_MODAL);
        OGRLayerProxy* new_layer =
        OGRDataAdapter::GetInstance().ExportDataSource(ds_format.ToStdString(),
			new_ds_name.ToStdString(), layername.ToStdString(), geom_type,
			ogr_geometries, table_int, selected_rows, spatial_ref, is_update);
        bool cont = true;
        while ( new_layer->export_progress < prog_n_max ) {
            cont = prog_dlg.Update(new_layer->export_progress);
            if ( !cont ) {
                new_layer->stop_exporting = true;
                OGRDataAdapter::GetInstance().CancelExport(new_layer);
                return;
            }
            if ( new_layer->export_progress == -1 ) {
                std::ostringstream msg;
                msg << "Exporting to data source (" << new_ds_name.ToStdString()
                << ") failed." << "\n\nDetails:"
                << new_layer->error_message.str();
                throw GdaException(msg.str().c_str());
            }
            wxMilliSleep(100);
        }
        OGRDataAdapter::GetInstance().StopExport();
        for (size_t i=0; i<geometries.size(); i++) {
            delete geometries[i];
        }
    } catch( GdaException& e ) {
        // clean intermedia memory
        for (size_t i=0; i < geometries.size(); i++) {
            delete geometries[i];
        }
        throw e;
    }
}

void Project::SpecifyProjectConfFile(const wxString& proj_fname)
{
	if (proj_fname.IsEmpty()) {
		throw GdaException("Project filename not specified.");
	}
	project_conf->SetFilePath(proj_fname);
	SetProjectFullPath(proj_fname);
}

void Project::SaveProjectConf()
{
	if (project_conf->GetFilePath().IsEmpty()) {
		throw GdaException("Project filename not specified.");
	}
	UpdateProjectConf();
	project_conf->Save(project_conf->GetFilePath());
}

void Project::SaveDataSourceData()
{
	LOG_MSG("Entering Project::SaveDataSourceData");
	
	// for some read-only datasources, suggest Export dialog
    GdaConst::DataSourceType ds_type = datasource->GetType();
    if (ds_type == GdaConst::ds_wfs ||
        ds_type == GdaConst::ds_kml ||
        ds_type == GdaConst::ds_esri_arc_sde ) {
        wxString msg = "The data source is read only. Please try to export "
        "to other data source.";
        throw GdaException(msg.mb_str());
    }
    
    if (table_int->ChangedSinceLastSave()) {
        wxString save_err_msg;
        try {
            // for saving changes in database, call OGRTableInterface::Save()
            if (ds_type == GdaConst::ds_shapefile ||
                ds_type == GdaConst::ds_dbf ||
                ds_type == GdaConst::ds_esri_file_geodb ||
                ds_type == GdaConst::ds_postgresql ||
                ds_type == GdaConst::ds_oci ||
                ds_type == GdaConst::ds_mysql) {
				
                table_int->Save(save_err_msg);
            }
            // for other datasources, call OGR interface to save.
            else {
                SaveOGRDataSource();
            }
        } catch( GdaException& e) {
            save_err_msg = e.what();
        }
        if (!save_err_msg.empty()) {
            table_int->SetChangedSinceLastSave(true);
			throw GdaException(save_err_msg.mb_str());
		} else {
            table_int->SetChangedSinceLastSave(false);
			SaveButtonManager* sbm = GetSaveButtonManager();
			if (sbm) sbm->SetDbSaveNeeded(false);
		}
    }
    
    if (IsTableOnlyProject() && main_data.records.size()>0
        && layer_proxy != NULL) {
        // case: create geometries for table-only datasource
        // try to save the geometries (e.g. database table)
        // NOTE: OGR/GDAL 2.0 is still implementing addGeomField feature.
        layer_proxy->AddGeometries(main_data);
    }	
	
	LOG_MSG("Exiting Project::SaveDataSourceData");
}

void Project::UpdateProjectConf()
{
	LOG_MSG("In Project::UpdateProjectConf");
	LayerConfiguration* layer_conf = project_conf->GetLayerConfiguration();
    datasource = layer_conf->GetDataSource();
	VarOrderPtree* var_order = layer_conf->GetVarOrderPtree();
	if (var_order) var_order->ReInitFromTableInt(table_int);
	CustomClassifPtree* cc = layer_conf->GetCustClassifPtree();
	if (cc) cc->SetCatClassifList(GetCatClassifManager());
	WeightsManPtree* spatial_weights = layer_conf->GetWeightsManPtree();
	if (spatial_weights) spatial_weights->SetWeightsMetaInfoList(GetWManager());
	DefaultVarsPtree* default_vars = layer_conf->GetDefaultVarsPtree();
	{
		std::vector<wxString> def_tm_ids(default_var_time.size());
		for (int t=0, sz=default_var_time.size(); t<sz; ++t) {
			def_tm_ids[t] = table_int->GetTimeString(default_var_time[t]);
		}
		if (default_vars) default_vars->SetDefaultVarList(default_var_name, 
														  def_tm_ids);
	}
}

wxString Project::GetProjectTitle()
{
	if (!project_title.IsEmpty()) return project_title;
	if (!layer_title.IsEmpty()) return layer_title;
	if (!layername.IsEmpty()) return layername;
    
    wxString current_proj_name = project_conf->GetFilePath();
	if (!current_proj_name.IsEmpty()) {
		wxFileName f(current_proj_name);
		return f.GetName();
	}
	return "";
}

void Project::ExportVoronoi()
{
	GetVoronoiPolygons();
	// generate a list of list of duplicates.  Or, better yet, have a map of
	// lists where the key is the id, and the value is a list of all ids
	// in the same set.
	// If duplicates exist, then give option to save duplicate sets to Table
	if (IsPointDuplicates()) DisplayPointDupsWarning();
	if (voronoi_polygons.size() != GetNumRecords()) return;
    
    ExportDataDlg dlg(NULL, voronoi_polygons, Shapefile::POLYGON, this);
    dlg.ShowModal();
}

/**
 * mean_centers == true: save mean centers
 * mean_centers == false: save centroids
 */
void Project::ExportCenters(bool is_mean_centers)
{	
	if (is_mean_centers) {
		GetMeanCenters();
        ExportDataDlg dlg(NULL, mean_centers, Shapefile::POINT, this);
        dlg.ShowModal();
	} else {
		GetCentroids();
        ExportDataDlg dlg(NULL, centroids, Shapefile::POINT, this);
        dlg.ShowModal();
	}
}

bool Project::IsPointDuplicates()
{
	if (!point_duplicates_initialized) {
		std::vector<double> x;
		std::vector<double> y;
		if (!GetCenters(x, y)) return false;
		Gda::VoronoiUtils::FindPointDuplicates(x, y, point_duplicates);
	}
	return point_duplicates.size() > 0;
}

void Project::DisplayPointDupsWarning()
{
	if (point_dups_warn_prev_displayed) return;
	wxString msg("Duplicate Thiessen polygons exist due "
				 "to duplicate or near-duplicate map points. "
				 " Press OK to save duplicate polygon ids "
				 "to Table.");
	wxMessageDialog dlg(NULL, msg, "Duplicate Thiessen "
						"Polygons Found",
						wxOK | wxCANCEL | wxICON_INFORMATION);
	if (dlg.ShowModal() == wxID_OK) SaveVoronoiDupsToTable();
	point_dups_warn_prev_displayed = true;
}

void Project::GetVoronoiRookNeighborMap(std::vector<std::set<int> >& nbr_map)
{
	IsPointDuplicates();
	std::vector<double> x;
	std::vector<double> y;
	GetCenters(x, y);
	Gda::VoronoiUtils::PointsToContiguity(x, y, false, nbr_map);
}

void Project::GetVoronoiQueenNeighborMap(std::vector<std::set<int> >& nbr_map)
{
	IsPointDuplicates();
	std::vector<double> x;
	std::vector<double> y;
	GetCenters(x, y);
	Gda::VoronoiUtils::PointsToContiguity(x, y, true, nbr_map);
}

GalElement* Project::GetVoronoiRookNeighborGal()
{
	if (!voronoi_rook_nbr_gal) {
		std::vector<std::set<int> > nbr_map;
		GetVoronoiRookNeighborMap(nbr_map);
		voronoi_rook_nbr_gal = Gda::VoronoiUtils::NeighborMapToGal(nbr_map);
	}
	return voronoi_rook_nbr_gal;
}

void Project::SaveVoronoiDupsToTable()
{
	if (!IsPointDuplicates()) return;
	std::vector<SaveToTableEntry> data(1);
	std::vector<wxInt64> dup_ids(num_records, -1);
	std::vector<bool> undefined(num_records, true);
	for (std::list<std::list<int> >::iterator dups_iter
		 = point_duplicates.begin();
		 dups_iter != point_duplicates.end(); dups_iter++) {
		int head_id = *(dups_iter->begin());
		for (std::list<int>::iterator iter=dups_iter->begin();
			 iter != dups_iter->end(); iter++) {
			undefined[*iter] = false;
			dup_ids[*iter] = head_id+1;
		}			
	}
	data[0].l_val = &dup_ids;
	data[0].undefined = &undefined;
	data[0].label = "Duplicate IDs";
	data[0].field_default = "DUP_IDS";
	data[0].type = GdaConst::long64_type;
	
	wxString title("Save Duplicate Thiessen Polygon Ids");
	SaveToTableDlg dlg(this, NULL, data, title,
					   wxDefaultPosition, wxSize(400,400));
	dlg.ShowModal();	
}

TableBase* Project::FindTableBase()
{
	using namespace std;
	if (!frames_manager) return 0;
	list<FramesManagerObserver*> observers(frames_manager->getCopyObservers());
	list<FramesManagerObserver*>::iterator it;
	for (it=observers.begin(); it != observers.end(); ++it) {
		if (TableFrame* w = dynamic_cast<TableFrame*>(*it)) {
			return w->GetTableBase();
		}
	}
	return 0;
}

void Project::GetSelectedRows(vector<int>& rowids)
{
    int n_rows = GetNumRecords();
    vector<bool>& hs = highlight_state->GetHighlight();
    for ( int i=0; i<n_rows; i++ ) {
        if (hs[i] ) rowids.push_back(i);
    }
}

wxGrid* Project::FindTableGrid()
{
	return FindTableBase()->GetView();
}

void Project::AddNeighborsToSelection()
{
	if (!GetWManager() || (GetWManager() && !GetWManager()->GetCurrWeight())) {
		return;
	}
	LOG_MSG("Entering Project::AddNeighborsToSelection");
	GalWeight* gal_weights = 0;

	GeoDaWeight* w = GetWManager()->GetCurrWeight();
	if (!w) {
		LOG_MSG("Warning: no current weight matrix found");
		return;
	}
	if (w->weight_type != GeoDaWeight::gal_type) {
		LOG_MSG("Error: Only GAL type weights are currently supported. "
				"Other weight types are internally converted to GAL.");
		return;
	} else {
		gal_weights = (GalWeight*) w;
	}
	
	// go through the list of all objects in current selection
	// for each selected object and add each of its neighbor to
	// the list so long as it isn't already selected.	
	
	HighlightState& hs = *highlight_state;
	std::vector<bool>& h = hs.GetHighlight();
	std::vector<int>& nh = hs.GetNewlyHighlighted();
	std::vector<int>& nuh = hs.GetNewlyUnhighlighted();
	int nh_cnt = 0;
	std::vector<bool> add_elem(gal_weights->num_obs, false);
	
	for (int i=0; i<gal_weights->num_obs; i++) {
		if (h[i]) {
			GalElement& e = gal_weights->gal[i];
			for (int j=0, jend=e.Size(); j<jend; j++) {
				int obs = e.elt(j);
				if (!h[obs] && !add_elem[obs]) {
					add_elem[obs] = true;
					nh[nh_cnt++] = obs;
				}
			}
		}
	}
	
	if (nh_cnt > 0) {
		hs.SetEventType(HighlightState::delta);
		hs.SetTotalNewlyHighlighted(nh_cnt);
		hs.SetTotalNewlyUnhighlighted(0);
		hs.notifyObservers();
	} else {
		LOG_MSG("No elements to add to current selection");
	}
	LOG_MSG("Exiting Project::AddNeighborsToSelection");
}

void Project::AddMeanCenters()
{
	LOG_MSG("In Project::AddMeanCenters");
	
	if (!table_int || main_data.records.size() == 0) return;
	GetMeanCenters();
	if (mean_centers.size() != num_records) return;

	std::vector<double> x(num_records, 0);
	std::vector<bool> x_undef(num_records, false);
	std::vector<double> y(num_records, 0);
	std::vector<bool> y_undef(num_records, false);
	for (int i=0; i<num_records; i++) {
		if (mean_centers[i]->isNull()) {
			x_undef[i] = true;
			y_undef[i] = true;
		} else {
			x[i] = mean_centers[i]->center_o.x;
			y[i] = mean_centers[i]->center_o.y;
		}
	}
	
	std::vector<SaveToTableEntry> data(2);
	data[0].d_val = &x;
	data[0].undefined = &x_undef;
	data[0].label = "X-Coordinates";
	data[0].field_default = "XMCTR";
	data[0].type = GdaConst::double_type;
	
	data[1].d_val = &y;
	data[1].undefined = &y_undef;
	data[1].label = "Y-Coordinates";
	data[1].field_default = "YMCTR";
	data[1].type = GdaConst::double_type;	
	
	SaveToTableDlg dlg(this, NULL, data,
					   "Add Mean Centers to Table",
					   wxDefaultPosition, wxSize(400,400));
	dlg.ShowModal();
}

void Project::AddCentroids()
{
	LOG_MSG("In Project::AddCentroids");
	
	if (!table_int || main_data.records.size() == 0) return;	
	GetCentroids();
	if (centroids.size() != num_records) return;
	
	std::vector<double> x(num_records);
	std::vector<bool> x_undef(num_records, false);
	std::vector<double> y(num_records);
	std::vector<bool> y_undef(num_records, false);
	for (int i=0; i<num_records; i++) {
		if (centroids[i]->isNull()) {
			x_undef[i] = true;
			y_undef[i] = true;
		} else {
			x[i] = centroids[i]->center_o.x;
			y[i] = centroids[i]->center_o.y;
		}
	}
	
	std::vector<SaveToTableEntry> data(2);
	data[0].d_val = &x;
	data[0].undefined = &x_undef;
	data[0].label = "X-Coordinates";
	data[0].field_default = "XCNTRD";
	data[0].type = GdaConst::double_type;
	
	data[1].d_val = &y;
	data[1].undefined = &y_undef;
	data[1].label = "Y-Coordinates";
	data[1].field_default = "YCNTRD";
	data[1].type = GdaConst::double_type;	
	
	SaveToTableDlg dlg(this, NULL, data,
					   "Add Centroids to Table",
					   wxDefaultPosition, wxSize(400,400));
	dlg.ShowModal();
}
	
bool Project::GetCenters(std::vector<double>& x, std::vector<double>& y)
{
	if (!main_data.records.size() == num_records) return false;
	x.resize(num_records);
	y.resize(num_records);
	
	const std::vector<GdaPoint*>& pts = GetCentroids();
	for (int i=0; i<num_records; i++) {
		x[i] = pts[i]->center_o.x;
		y[i] = pts[i]->center_o.y;
	}
	return true;
}

const std::vector<GdaPoint*>& Project::GetMeanCenters()
{
	int num_obs = main_data.records.size();
	if (mean_centers.size() == 0 && num_obs > 0) {
		if (main_data.header.shape_type == Shapefile::POINT) {
			mean_centers.resize(num_obs);
			Shapefile::PointContents* pc;
			for (int i=0; i<num_obs; i++) {
				pc = (Shapefile::PointContents*)
					main_data.records[i].contents_p;
				if (pc->shape_type == 0) {
					mean_centers[i] = new GdaPoint(wxRealPoint(pc->x, pc->y));
				} else {
					mean_centers[i] = new GdaPoint();
				}
			}
		} else if (main_data.header.shape_type == Shapefile::POLYGON) {
			mean_centers.resize(num_obs);
			Shapefile::PolygonContents* pc;
			for (int i=0; i<num_obs; i++) {
				pc = (Shapefile::PolygonContents*)
					main_data.records[i].contents_p;
				GdaPolygon poly(pc);
				if (poly.isNull()) {
					mean_centers[i] = new GdaPoint();
				} else {
					mean_centers[i] =
						new GdaPoint(GdaShapeAlgs::calculateMeanCenter(&poly));
				}
			}
		}
	}
	return mean_centers;
}

void Project::GetMeanCenters(std::vector<double>& x, std::vector<double>& y)
{
    GetMeanCenters();
    int num_obs = mean_centers.size();
    if (x.size() < num_obs) x.resize(num_obs);
    if (y.size() < num_obs) y.resize(num_obs);
    for (int i=0; i<num_obs; i++) {
        x[i] = mean_centers[i]->center_o.x;
        y[i] = mean_centers[i]->center_o.y;
    }
}

const std::vector<GdaPoint*>& Project::GetCentroids()
{
	int num_obs = main_data.records.size();
	if (centroids.size() == 0 && num_obs > 0) {
		if (main_data.header.shape_type == Shapefile::POINT) {
			centroids.resize(num_obs);
			Shapefile::PointContents* pc;
			for (int i=0; i<num_obs; i++) {
				pc = (Shapefile::PointContents*)
					main_data.records[i].contents_p;
				if (pc->shape_type == 0) {
					centroids[i] = new GdaPoint();
				} else {
					centroids[i] = new GdaPoint(wxRealPoint(pc->x, pc->y));
				}
			}
		} else if (main_data.header.shape_type == Shapefile::POLYGON) {
			centroids.resize(num_obs);
			Shapefile::PolygonContents* pc;
			for (int i=0; i<num_obs; i++) {
				pc = (Shapefile::PolygonContents*)
					main_data.records[i].contents_p;
				GdaPolygon poly(pc);
				if (poly.isNull()) {
					centroids[i] = new GdaPoint();
				} else {
					centroids[i] =
						new GdaPoint(GdaShapeAlgs::calculateCentroid(&poly));
				}
			}
		}
	}
	return centroids;	
}

void Project::GetCentroids(std::vector<double>& x, std::vector<double>& y)
{
	GetCentroids();
	int num_obs = centroids.size();
	if (x.size() < num_obs) x.resize(num_obs);
	if (y.size() < num_obs) y.resize(num_obs);
	for (int i=0; i<num_obs; i++) {
		x[i] = centroids[i]->center_o.x;
		y[i] = centroids[i]->center_o.y;
	}
}

const std::vector<GdaShape*>& Project::GetVoronoiPolygons()
{
	if (voronoi_polygons.size() == num_records) {
		return voronoi_polygons;
	} else {
		for (int i=0; i<voronoi_polygons.size(); i++) {
			delete voronoi_polygons[i];
		}
		voronoi_polygons.clear();
	}
	
	std::vector<double> x(num_records);
	std::vector<double> y(num_records);
	GetCenters(x,y);
	
	Gda::VoronoiUtils::MakePolygons(x, y, voronoi_polygons,
									  voronoi_bb_xmin, voronoi_bb_ymin,
									  voronoi_bb_xmax, voronoi_bb_ymax);
	for (size_t i=0, iend=voronoi_polygons.size(); i<iend; i++) {
		voronoi_polygons[i]->setPen(*wxBLACK_PEN);
		voronoi_polygons[i]->setBrush(*wxTRANSPARENT_BRUSH);
	}
	
	return voronoi_polygons;
}

wxString Project::GetDefaultVarName(int var)
{
	if (var >= 0 && var < default_var_name.size()) return default_var_name[var];
	return "";
}

void Project::SetDefaultVarName(int var, const wxString& v_name)
{
	if (var >= 0 && var < default_var_name.size()) {
		default_var_name[var] = v_name;
	}
}

int Project::GetDefaultVarTime(int var)
{
	if (var >= 0 && var < default_var_time.size()) return default_var_time[var];
	return 0;
}

void Project::SetDefaultVarTime(int var, int time)
{
	if (var >= 0 && var < default_var_time.size()) default_var_time[var] = time;
}

/** This should only be called by the Project constructors.  This represents
 the common part a New Project object once certain initial parts are
 initialized differently. */
bool Project::CommonProjectInit()
{	
	// If dBase or Shapefile, process as dBase, otherwise process as OGR source
	if (datasource->GetType() == GdaConst::ds_dbf ||
		datasource->GetType() == GdaConst::ds_shapefile) {
		if (!InitFromShapefileLayer()) return false;
	} else {
		if (!InitFromOgrLayer()) return false;
	}
    
	num_records = table_int->GetNumberRows();
	
	// Initialize various managers
	save_manager = new SaveButtonManager(GetTableState());
	frames_manager = new FramesManager;
	highlight_state = new HighlightState;
	cat_classif_manager = new CatClassifManager(table_int, GetTableState(),
        project_conf->GetLayerConfiguration()->GetCustClassifPtree());
	highlight_state->SetSize(num_records);
	w_manager = new WeightsManager(this);
	WeightsManPtree* spatial_weights =
		project_conf->GetLayerConfiguration()->GetWeightsManPtree();
	if (spatial_weights) {
		w_manager->InitFromMetaInfo(spatial_weights->GetWeightsMetaInfoList(),
									table_int);
	}
	for (int i=0; i<4; i++) {
		default_var_name[i] = "";
		default_var_time[i] = 0;
	}
	DefaultVarsPtree* default_vars =
		project_conf->GetLayerConfiguration()->GetDefaultVarsPtree();
	if (default_vars) {
		int i=0;
		std::vector<wxString> tm_strs;
		table_int->GetTimeStrings(tm_strs);
		std::map<wxString, int> tm_map;
		for (int t=0, sz=tm_strs.size(); t<sz; ++t) tm_map[tm_strs[t]] = t;
		BOOST_FOREACH(const DefaultVar& dv, default_vars->GetDefaultVarList()) {
			if (!table_int->DoesNameExist(dv.name, false)) {
				default_var_name[i] = "";
				default_var_time[i] = 0;
			} else {
				default_var_name[i] = dv.name;
				if (!dv.time_id.IsEmpty()) {
					if (tm_map.find(dv.time_id) != tm_map.end()) {
						default_var_time[i] = tm_map[dv.time_id];
					} else {
						default_var_time[i] = 0;
					}
				}
			}
			if (i < default_var_name.size()) ++i;
		}
	}
	std::vector<wxString> ts;
	table_int->GetTimeStrings(ts);
	time_state->SetTimeIds(ts);
	shared_category_scratch.resize(
			boost::extents[CatClassification::max_num_categories][num_records]);
	
	// No DB or Meta data could have changed by this point, so ensure
	// Save buttons disabled.
	save_manager->SetMetaDataSaveNeeded(false);
	save_manager->SetDbSaveNeeded(false);
	
	// MMM: SaveButtonManager revisit
	// MMM: Move this to SaveButtonManager
	// Enable "Save" only for DBF data sources.  "Save As" is always enabled.
	//allow_enable_save = false;
	//if (DbfTable* o = dynamic_cast<DbfTable*>(table_int)) {
	//	allow_enable_save = wxFileExists(o->GetDbfFileName().GetFullPath());
	//} else if (OGRTable* o = dynamic_cast<OGRTable*>(table_int)) {
	//	allow_enable_save = !o->IsReadOnly();
	//}
	return true;
}

/** 
 * Initialize the Table and Shape Layer from Shapefile / DBF 
 */
bool Project::InitFromShapefileLayer()
{
	LOG_MSG("Entering Project::InitFromShapefileLayer");
	bool table_only_project = IsTableOnlyProject();
	LOG(table_only_project);
	
	wxString ds_fname = datasource->GetOGRConnectStr();
    datasource->UpdateWritable(true);
	wxString shp_fname;
	wxString shx_fname;
	wxString dbf_fname;
	if (table_only_project) {
		dbf_fname = ds_fname;
	} else {
		shp_fname = ds_fname;
		LOG(shp_fname);
		wxFileName fname(shp_fname);
		dbf_fname = fname.GetPathWithSep() + fname.GetName() + ".dbf";
		shx_fname = fname.GetPathWithSep() + fname.GetName() + ".shx";
	}
	LOG(dbf_fname);
	
	if (datasource->GetType() == GdaConst::ds_shapefile) {
		// check shp file first
		if (IsLineShapeFile(shp_fname)) {
			open_err_msg << "GeoDa does not support Shapefiles ";
			open_err_msg << "with line data at this time.  Please choose a ";
			open_err_msg << "Shapefile with either point or polygon data.";
			return false;
		}
		// check shx/dbf files
		bool shx_found;
		bool dbf_found;
		
		if (!GenUtils::ExistsShpShxDbf(shp_fname,
									   0, &shx_found, &dbf_found)) {
			open_err_msg << "Error: " << shp_fname << ", ";
			open_err_msg << shx_fname << ", and ";
			open_err_msg << dbf_fname;
			open_err_msg << " were not found together in the same file ";
			open_err_msg << "directory. Could not find ";
			if (!shx_found && dbf_found) {
				open_err_msg << shx_fname << ".";
			} else if (shx_found && !dbf_found) {
				open_err_msg << dbf_fname << ".";
			} else {
				open_err_msg << shx_fname << " and ";
				open_err_msg << dbf_fname << ".";
			}
			return false;
		}
	}
	DbfFileReader dbf(dbf_fname);
	if (!dbf.isDbfReadSuccess()) {
		open_err_msg << "Failed to read DBF file.";
		return false;
	}
	std::map<wxString, GdaConst::FieldType> var_type_map;
	dbf.getFieldTypes(var_type_map);
	std::vector<wxString> var_list;
	dbf.getFieldList(var_list);
	
	// Correct variable order information
	LayerConfiguration* layer_conf = project_conf->GetLayerConfiguration();
	VarOrderPtree* variable_order = layer_conf->GetVarOrderPtree();
	variable_order->CorrectVarGroups(var_type_map, var_list);

	table_state = new TableState;
	time_state = new TimeState;
	table_int = new DbfTable(table_state, time_state, dbf, *variable_order);
	if (!table_int) {
		open_err_msg << "There was a problem reading the table";
		delete table_state;
		return false;
	}
	if (!table_int->IsValid()) {
		open_err_msg = table_int->GetOpenErrorMessage();
		delete table_state;
		delete table_int;
		return false;
	}
	if (!table_only_project) {
		// Read shape layer
		return OpenShpFile(shp_fname);
	}
	LOG_MSG("Exiting Project::InitFromShapefileLayer");
	return true;
}

/** Initialize the Table and Shape Layer from OGR source */
bool Project::InitFromOgrLayer()
{
	LOG_MSG("Entering Project::InitFromOgrLayer");
	wxString datasource_name = datasource->GetOGRConnectStr();
	
	// OK. ReadLayer() is running in a seperate thread.
	// That gives us a chance to get its progress from a Progress window.
	layer_proxy =
	    OGRDataAdapter::GetInstance()
	       .T_ReadLayer(datasource_name.ToStdString(),layername.ToStdString());

    OGRwkbGeometryType eGType = layer_proxy->GetShapeType();
    if ( eGType == wkbLineString || eGType == wkbMultiLineString ) {
        open_err_msg << "GeoDa does not support datasource ";
		open_err_msg << "with line data at this time.  Please choose a ";
		open_err_msg << "datasource with either point or polygon data.";
		throw GdaException(open_err_msg.c_str());
		return false;
    }
    
	int prog_n_max = layer_proxy->n_rows;
	if (prog_n_max <= 0) prog_n_max = 2;
	wxProgressDialog prog_dlg("Open data source progress dialog",
							  "Loading data...", prog_n_max,  NULL,
							  wxPD_CAN_ABORT | wxPD_AUTO_HIDE | wxPD_APP_MODAL);
	bool cont = true;
	while (( layer_proxy->load_progress < layer_proxy->n_rows
		     || layer_proxy->n_rows <= 0 ) && !layer_proxy->HasError())
	{
		if (layer_proxy->n_rows == -1) {
			// if cannot get n_rows, make the progress stay at the half position
			cont = prog_dlg.Update(1);
		} else{
            if (layer_proxy->load_progress >= 0 &&
                layer_proxy->load_progress < prog_n_max )
                cont = prog_dlg.Update(layer_proxy->load_progress);
		}
		if (!cont)  {
			OGRDataAdapter::GetInstance().T_StopReadLayer(layer_proxy);
			return false;
		}
		wxMilliSleep(100);
	}
	if (!layer_proxy) {
		open_err_msg << "There was a problem reading the layer";
		throw GdaException(open_err_msg.c_str());
		return false;
	} else if ( layer_proxy->HasError() ) {
		open_err_msg << layer_proxy->error_message.str();
		throw GdaException(open_err_msg.c_str());
		return false;
    }
    OGRDatasourceProxy* ds_proxy =
	    OGRDataAdapter::GetInstance().GetDatasourceProxy(
												datasource_name.ToStdString());
    datasource->UpdateWritable(ds_proxy->is_writable);
	// Correct variable_order information, which will be used by OGRTable
	std::vector<wxString> var_list;
	std::map<wxString, GdaConst::FieldType> var_type_map;
	layer_proxy->GetVarTypeMap(var_list, var_type_map);
	
	LayerConfiguration* layer_conf = project_conf->GetLayerConfiguration();
	VarOrderPtree* variable_order = layer_conf->GetVarOrderPtree();
	variable_order->CorrectVarGroups(var_type_map, var_list);
	
	table_state = new TableState;
	time_state = new TimeState;
	table_int = new OGRTable(layer_proxy, datasource->GetType(),
                             table_state, time_state, 
                             *variable_order);
	if (!table_int) {
		open_err_msg << "There was a problem reading the table";
		delete table_state;
		throw GdaException(open_err_msg.c_str());
		return false;
	}
	if (!table_int->IsValid()) {
		open_err_msg = table_int->GetOpenErrorMessage();
		delete table_state;
		delete table_int;
		return false;
	}
	
	if (!IsTableOnlyProject()) {
		layer_proxy->ReadGeometries(main_data);
	}
	// run caching in background
	//OGRDataAdapter::GetInstance().CacheLayer
    //(ds_name.ToStdString(), layer_name.ToStdString(), layer_proxy);
	LOG_MSG("Exiting Project::InitFromOgrLayer");
	return true;
}

bool Project::IsTableOnlyProject()
{
    if (datasource->GetType() == GdaConst::ds_dbf ||
		datasource->GetType() == GdaConst::ds_shapefile) {
		return datasource->GetType() == GdaConst::ds_dbf;
	} else {
		return layer_proxy->IsTableOnly();
	}
}

bool Project::OpenShpFile(wxFileName shp_fname)
{
	LOG_MSG("Entering Project::OpenShpFile");
	using namespace std;
	using namespace Shapefile;
	
	wxFileName m_shx_fname(shp_fname);
	m_shx_fname.SetExt("shx");
	wxString m_shx_str = m_shx_fname.GetFullPath();
	
	wxFileName m_shp_fname(shp_fname);
	m_shp_fname.SetExt("shp");
	wxString m_shp_str = m_shp_fname.GetFullPath();
	
	Shapefile::Index index_data;
	bool success = Shapefile::populateIndex(m_shx_str, index_data);
	
	if (success) {	
		success = Shapefile::populateMain(index_data, m_shp_str, main_data);
		
		if (index_data.header.shape_type == POLYGON_Z) {
			index_data.header.shape_type = POLYGON;
		} else if (index_data.header.shape_type == POLYGON_M) {  
			index_data.header.shape_type = POLYGON;
		} else if (index_data.header.shape_type == POINT_Z) {
			index_data.header.shape_type = Shapefile::POINT;
		} else if (index_data.header.shape_type == POINT_M) {
			index_data.header.shape_type = Shapefile::POINT;
		} else if (index_data.header.shape_type == POLY_LINE_Z) {
			index_data.header.shape_type = POLY_LINE;
		} else if (index_data.header.shape_type == POLY_LINE_M) {
			index_data.header.shape_type = POLY_LINE;
		}
	}
	
	if (!success) {
		// display a failure window if unsupported shapefile type encountered
		return false;
	}
	
    int shp_num_recs = Shapefile::calcNumIndexHeaderRecords(index_data.header);
    LOG(shp_num_recs);
	LOG(main_data.records.size());
	LOG_MSG("Exiting Project::OpenShpFile");
	return true;
}

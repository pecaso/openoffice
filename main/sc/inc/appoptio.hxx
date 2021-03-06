/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



#ifndef SC_APPOPTIO_HXX
#define SC_APPOPTIO_HXX

#include <vcl/field.hxx>
#include <svx/zoomitem.hxx>
#include <unotools/configitem.hxx>
#include "scdllapi.h"
#include "global.hxx"
#include "optutil.hxx"

class SC_DLLPUBLIC ScAppOptions
{
public:
				ScAppOptions();
				ScAppOptions( const ScAppOptions& rCpy );
				~ScAppOptions();

	void		SetDefaults();

	void		SetAppMetric( FieldUnit eUnit )	{ eMetric = eUnit;		}
	FieldUnit	GetAppMetric() const			{ return eMetric;		}
	void		SetZoom( sal_uInt16 nNew )			{ nZoom = nNew;			}
	sal_uInt16		GetZoom() const					{ return nZoom;			}
	void		SetZoomType( SvxZoomType eNew ) { eZoomType = eNew;		}
	SvxZoomType	GetZoomType() const				{ return eZoomType;		}
    void        SetSynchronizeZoom( sal_Bool bNew ) { bSynchronizeZoom = bNew; }
    sal_Bool        GetSynchronizeZoom() const      { return bSynchronizeZoom; }
	sal_uInt16		GetLRUFuncListCount() const		{ return nLRUFuncCount;	}
	sal_uInt16*		GetLRUFuncList() const			{ return pLRUList;		}
	void		SetLRUFuncList( const sal_uInt16* pList,
								const sal_uInt16  nCount );
	void		SetStatusFunc( sal_uInt16 nNew )	{ nStatusFunc = nNew;	}
	sal_uInt16		GetStatusFunc() const			{ return nStatusFunc;	}
	void		SetAutoComplete( sal_Bool bNew )	{ bAutoComplete = bNew; }
	sal_Bool		GetAutoComplete() const			{ return bAutoComplete;	}
	void		SetDetectiveAuto( sal_Bool bNew )	{ bDetectiveAuto = bNew; }
	sal_Bool		GetDetectiveAuto() const		{ return bDetectiveAuto; }

	void		SetTrackContentColor(sal_uInt32 nNew) { nTrackContentColor = nNew; }
	sal_uInt32	GetTrackContentColor() const	 { return nTrackContentColor; }
	void		SetTrackInsertColor(sal_uInt32 nNew)  { nTrackInsertColor = nNew;  }
	sal_uInt32	GetTrackInsertColor() const		 { return nTrackInsertColor;  }
	void		SetTrackDeleteColor(sal_uInt32 nNew)  { nTrackDeleteColor = nNew;  }
	sal_uInt32	GetTrackDeleteColor() const		 { return nTrackDeleteColor;  }
	void		SetTrackMoveColor(sal_uInt32 nNew)    { nTrackMoveColor = nNew;    }
	sal_uInt32	GetTrackMoveColor() const		 { return nTrackMoveColor;    }

	ScLkUpdMode GetLinkMode() const				{ return eLinkMode ;}
	void		SetLinkMode( ScLkUpdMode nSet )	{ 	eLinkMode  = nSet;}

	void		SetDefaultObjectSizeWidth(sal_Int32 nNew)	{ nDefaultObjectSizeWidth = nNew; }
	sal_Int32		GetDefaultObjectSizeWidth() const		{ return nDefaultObjectSizeWidth; }
	void		SetDefaultObjectSizeHeight(sal_Int32 nNew)	{ nDefaultObjectSizeHeight = nNew; }
	sal_Int32		GetDefaultObjectSizeHeight() const		{ return nDefaultObjectSizeHeight; }

    void        SetShowSharedDocumentWarning( sal_Bool bNew )   { mbShowSharedDocumentWarning = bNew; }
    sal_Bool        GetShowSharedDocumentWarning() const        { return mbShowSharedDocumentWarning; }


	const ScAppOptions&	operator=	( const ScAppOptions& rOpt );

private:
	FieldUnit	eMetric;
	sal_uInt16		nLRUFuncCount;
	sal_uInt16*		pLRUList;
	SvxZoomType eZoomType;
	sal_uInt16		nZoom;
    sal_Bool        bSynchronizeZoom;
	sal_uInt16		nStatusFunc;
	sal_Bool		bAutoComplete;
	sal_Bool		bDetectiveAuto;
	sal_uInt32	nTrackContentColor;
	sal_uInt32	nTrackInsertColor;
	sal_uInt32	nTrackDeleteColor;
	sal_uInt32	nTrackMoveColor;
	ScLkUpdMode eLinkMode;
	sal_Int32		nDefaultObjectSizeWidth;
	sal_Int32		nDefaultObjectSizeHeight;
    sal_Bool        mbShowSharedDocumentWarning;
};


//==================================================================
//	Config Item containing app options
//==================================================================

class ScAppCfg : public ScAppOptions
{
	//	spread about 5 config paths
	//!	split ScAppOptions into different classes

	ScLinkConfigItem	aLayoutItem;
	ScLinkConfigItem	aInputItem;
	ScLinkConfigItem	aRevisionItem;
	ScLinkConfigItem	aContentItem;
	ScLinkConfigItem	aSortListItem;
	ScLinkConfigItem	aMiscItem;

	DECL_LINK( LayoutCommitHdl, void* );
	DECL_LINK( InputCommitHdl, void* );
	DECL_LINK( RevisionCommitHdl, void* );
	DECL_LINK( ContentCommitHdl, void* );
	DECL_LINK( SortListCommitHdl, void* );
	DECL_LINK( MiscCommitHdl, void* );

	com::sun::star::uno::Sequence<rtl::OUString> GetLayoutPropertyNames();
	com::sun::star::uno::Sequence<rtl::OUString> GetInputPropertyNames();
	com::sun::star::uno::Sequence<rtl::OUString> GetRevisionPropertyNames();
	com::sun::star::uno::Sequence<rtl::OUString> GetContentPropertyNames();
	com::sun::star::uno::Sequence<rtl::OUString> GetSortListPropertyNames();
	com::sun::star::uno::Sequence<rtl::OUString> GetMiscPropertyNames();

public:
			ScAppCfg();

	void	SetOptions( const ScAppOptions& rNew );
	void	OptionsChanged();	// after direct access to ScAppOptions base class
};


#endif


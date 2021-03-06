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



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_sc.hxx"




// INCLUDE ---------------------------------------------------------------

#include <algorithm>
#include <vector>
#include <set>
#include <hash_map>
#include <hash_set>

#include <tools/debug.hxx>
#include <rtl/math.hxx>
#include <svl/itemprop.hxx>
#include <svl/intitem.hxx>

#include "scitems.hxx"
#include "document.hxx"
#include "docpool.hxx"
#include "patattr.hxx"
#include "cell.hxx"

#include "dptabsrc.hxx"
#include "dptabres.hxx"
#include "dptabdat.hxx"
#include "global.hxx"
#include "collect.hxx"
#include "datauno.hxx"		// ScDataUnoConversion
#include "unoguard.hxx"
#include "miscuno.hxx"
#include "unonames.hxx"

#include <com/sun/star/beans/PropertyAttribute.hpp>
#include <com/sun/star/sheet/DataPilotFieldFilter.hpp>
#include <com/sun/star/sheet/DataPilotFieldOrientation.hpp>
#include <com/sun/star/sheet/DataPilotFieldFilter.hpp>
#include <com/sun/star/sheet/DataPilotFieldReferenceType.hpp>
#include <com/sun/star/sheet/DataPilotFieldSortMode.hpp>
#include <com/sun/star/sheet/DataPilotFieldAutoShowInfo.hpp>
#include <com/sun/star/table/CellAddress.hpp>

#include <unotools/collatorwrapper.hxx>
#include <unotools/calendarwrapper.hxx>
#include <com/sun/star/i18n/CalendarDisplayIndex.hpp>

using namespace com::sun::star;
using ::std::vector;
using ::std::set;
using ::std::hash_map;
using ::std::hash_set;
using ::com::sun::star::uno::Reference;
using ::com::sun::star::uno::Sequence;
using ::com::sun::star::uno::Any;
using ::com::sun::star::sheet::DataPilotFieldAutoShowInfo;
using ::rtl::OUString;

// -----------------------------------------------------------------------

#define SC_MINCOUNT_LIMIT	1000000

// -----------------------------------------------------------------------

SC_SIMPLE_SERVICE_INFO( ScDPSource,      "ScDPSource",      "com.sun.star.sheet.DataPilotSource" )
SC_SIMPLE_SERVICE_INFO( ScDPDimensions,  "ScDPDimensions",  "com.sun.star.sheet.DataPilotSourceDimensions" )
SC_SIMPLE_SERVICE_INFO( ScDPDimension,   "ScDPDimension",   "com.sun.star.sheet.DataPilotSourceDimension" )
SC_SIMPLE_SERVICE_INFO( ScDPHierarchies, "ScDPHierarchies", "com.sun.star.sheet.DataPilotSourceHierarcies" )
SC_SIMPLE_SERVICE_INFO( ScDPHierarchy,   "ScDPHierarchy",   "com.sun.star.sheet.DataPilotSourceHierarcy" )
SC_SIMPLE_SERVICE_INFO( ScDPLevels,      "ScDPLevels",      "com.sun.star.sheet.DataPilotSourceLevels" )
SC_SIMPLE_SERVICE_INFO( ScDPLevel,       "ScDPLevel",       "com.sun.star.sheet.DataPilotSourceLevel" )
SC_SIMPLE_SERVICE_INFO( ScDPMembers,     "ScDPMembers",     "com.sun.star.sheet.DataPilotSourceMembers" )
SC_SIMPLE_SERVICE_INFO( ScDPMember,      "ScDPMember",      "com.sun.star.sheet.DataPilotSourceMember" )

// -----------------------------------------------------------------------

// property maps for PropertySetInfo
//	DataDescription / NumberFormat are internal

// -----------------------------------------------------------------------

//!	move to a header?
sal_Bool lcl_GetBoolFromAny( const uno::Any& aAny )
{
	if ( aAny.getValueTypeClass() == uno::TypeClass_BOOLEAN )
		return *(sal_Bool*)aAny.getValue();
	return sal_False;
}

void lcl_SetBoolInAny( uno::Any& rAny, sal_Bool bValue )
{
	rAny.setValue( &bValue, getBooleanCppuType() );
}

// -----------------------------------------------------------------------

ScDPSource::ScDPSource( ScDPTableData* pD ) :
	pData( pD ),
	pDimensions( NULL ),
	nColDimCount( 0 ),
	nRowDimCount( 0 ),
	nDataDimCount( 0 ),
	nPageDimCount( 0 ),
	bColumnGrand( sal_True ),		// default is true
	bRowGrand( sal_True ),
	bIgnoreEmptyRows( sal_False ),
	bRepeatIfEmpty( sal_False ),
	nDupCount( 0 ),
	pResData( NULL ),
	pColResRoot( NULL ),
	pRowResRoot( NULL ),
	pColResults( NULL ),
	pRowResults( NULL ),
	bResultOverflow( sal_False ),
    bPageFiltered( false ),
    mpGrandTotalName(NULL)
{
	pData->SetEmptyFlags( bIgnoreEmptyRows, bRepeatIfEmpty );
}

ScDPSource::~ScDPSource()
{
	if (pDimensions)
		pDimensions->release();		// ref-counted

	//!	free lists

	delete[] pColResults;
	delete[] pRowResults;

	delete pColResRoot;
	delete pRowResRoot;
	delete pResData;
}

void ScDPSource::SetGrandTotalName(const ::rtl::OUString& rName)
{
    mpGrandTotalName.reset(new ::rtl::OUString(rName));
}

const ::rtl::OUString* ScDPSource::GetGrandTotalName() const
{
    return mpGrandTotalName.get();
}

sal_uInt16 ScDPSource::GetOrientation(long nColumn)
{
	long i;
	for (i=0; i<nColDimCount; i++)
		if (nColDims[i] == nColumn)
			return sheet::DataPilotFieldOrientation_COLUMN;
	for (i=0; i<nRowDimCount; i++)
		if (nRowDims[i] == nColumn)
			return sheet::DataPilotFieldOrientation_ROW;
	for (i=0; i<nDataDimCount; i++)
		if (nDataDims[i] == nColumn)
			return sheet::DataPilotFieldOrientation_DATA;
	for (i=0; i<nPageDimCount; i++)
		if (nPageDims[i] == nColumn)
			return sheet::DataPilotFieldOrientation_PAGE;
	return sheet::DataPilotFieldOrientation_HIDDEN;
}

long ScDPSource::GetDataDimensionCount()
{
	return nDataDimCount;
}

ScDPDimension* ScDPSource::GetDataDimension(long nIndex)
{
    if (nIndex < 0 || nIndex >= nDataDimCount)
        return NULL;

    long nDimIndex = nDataDims[nIndex];
    return GetDimensionsObject()->getByIndex(nDimIndex);
}

String ScDPSource::GetDataDimName( long nIndex )
{
	String aRet;
    ScDPDimension* pDim = GetDataDimension(nIndex);
    if (pDim)
        aRet = String(pDim->getName());
	return aRet;
}

long ScDPSource::GetPosition(long nColumn)
{
	long i;
	for (i=0; i<nColDimCount; i++)
		if (nColDims[i] == nColumn)
			return i;
	for (i=0; i<nRowDimCount; i++)
		if (nRowDims[i] == nColumn)
			return i;
	for (i=0; i<nDataDimCount; i++)
		if (nDataDims[i] == nColumn)
			return i;
	for (i=0; i<nPageDimCount; i++)
		if (nPageDims[i] == nColumn)
			return i;
	return 0;
}

sal_Bool lcl_TestSubTotal( sal_Bool& rAllowed, long nColumn, long* pArray, long nCount, ScDPSource* pSource )
{
	for (long i=0; i<nCount; i++)
		if (pArray[i] == nColumn)
		{
			//	no subtotals for data layout dim, no matter where
			if ( pSource->IsDataLayoutDimension(nColumn) )
				rAllowed = sal_False;
			else
			{
				//	no subtotals if no other dim but data layout follows
				long nNextIndex = i+1;
				if ( nNextIndex < nCount && pSource->IsDataLayoutDimension(pArray[nNextIndex]) )
					++nNextIndex;
				if ( nNextIndex >= nCount )
					rAllowed = sal_False;
			}

			return sal_True;	// found
		}
	return sal_False;
}

sal_Bool ScDPSource::SubTotalAllowed(long nColumn)
{
	//!	cache this at ScDPResultData
	sal_Bool bAllowed = sal_True;
	if ( lcl_TestSubTotal( bAllowed, nColumn, nColDims, nColDimCount, this ) )
		return bAllowed;
	if ( lcl_TestSubTotal( bAllowed, nColumn, nRowDims, nRowDimCount, this ) )
		return bAllowed;
	return bAllowed;
}

void lcl_RemoveDim( long nRemove, long* pDims, long& rCount )
{
	for (long i=0; i<rCount; i++)
		if ( pDims[i] == nRemove )
		{
			for (long j=i; j+1<rCount; j++)
				pDims[j] = pDims[j+1];
			--rCount;
			return;
		}
}

void ScDPSource::SetOrientation(long nColumn, sal_uInt16 nNew)
{
	//!	change to no-op if new orientation is equal to old?

	// remove from old list
	lcl_RemoveDim( nColumn, nColDims, nColDimCount );
	lcl_RemoveDim( nColumn, nRowDims, nRowDimCount );
	lcl_RemoveDim( nColumn, nDataDims, nDataDimCount );
	lcl_RemoveDim( nColumn, nPageDims, nPageDimCount );

	// add to new list
	switch (nNew)
	{
		case sheet::DataPilotFieldOrientation_COLUMN:
			nColDims[nColDimCount++] = nColumn;
			break;
		case sheet::DataPilotFieldOrientation_ROW:
			nRowDims[nRowDimCount++] = nColumn;
			break;
		case sheet::DataPilotFieldOrientation_DATA:
			nDataDims[nDataDimCount++] = nColumn;
			break;
		case sheet::DataPilotFieldOrientation_PAGE:
			nPageDims[nPageDimCount++] = nColumn;
			break;
            // Wang Xu Ming -- 2009-9-1
            // DataPilot Migration - Cache&&Performance
        case sheet::DataPilotFieldOrientation_HIDDEN:
            break;
            // End Comments
		default: 
			DBG_ERROR( "ScDPSource::SetOrientation: unexpected orientation" );
			break;
	}
}

sal_Bool ScDPSource::IsDataLayoutDimension(long nDim)
{
	return nDim == pData->GetColumnCount();
}

sal_uInt16 ScDPSource::GetDataLayoutOrientation()
{
	return GetOrientation(pData->GetColumnCount());
}

sal_Bool ScDPSource::IsDateDimension(long nDim)
{
	return pData->IsDateDimension(nDim);
}

sal_uInt32	ScDPSource::GetNumberFormat(long nDim)
{
	return pData->GetNumberFormat( nDim );
}

ScDPDimensions* ScDPSource::GetDimensionsObject()
{
	if (!pDimensions)
	{
		pDimensions = new ScDPDimensions(this);
		pDimensions->acquire();						// ref-counted
	}
	return pDimensions;
}

uno::Reference<container::XNameAccess> SAL_CALL ScDPSource::getDimensions() throw(uno::RuntimeException)
{
	return GetDimensionsObject();
}

void ScDPSource::SetDupCount( long nNew )
{
	nDupCount = nNew;
}

ScDPDimension* ScDPSource::AddDuplicated(long /* nSource */, const String& rNewName)
{
	DBG_ASSERT( pDimensions, "AddDuplicated without dimensions?" );

	//	re-use

	long nOldDimCount = pDimensions->getCount();
	for (long i=0; i<nOldDimCount; i++)
	{
		ScDPDimension* pDim = pDimensions->getByIndex(i);
		if (pDim && String(pDim->getName()) == rNewName)
		{
			//!	test if pDim is a duplicate of source
			return pDim;
		}
	}

	SetDupCount( nDupCount + 1 );
	pDimensions->CountChanged();		// uses nDupCount

	return pDimensions->getByIndex( pDimensions->getCount() - 1 );
}

long ScDPSource::GetSourceDim(long nDim)
{
	//	original source dimension or data layout dimension?
	if ( nDim <= pData->GetColumnCount() )
		return nDim;

	if ( nDim < pDimensions->getCount() )
	{
		ScDPDimension* pDimObj = pDimensions->getByIndex( nDim );
		if ( pDimObj )
		{
			long nSource = pDimObj->GetSourceDim();
			if ( nSource >= 0 )
				return nSource;
		}
	}

	DBG_ERROR("GetSourceDim: wrong dim");
	return nDim;
}

uno::Sequence< uno::Sequence<sheet::DataResult> > SAL_CALL ScDPSource::getResults()
															throw(uno::RuntimeException)
{
	CreateRes_Impl();		// create pColResRoot and pRowResRoot

	if ( bResultOverflow )		// set in CreateRes_Impl
	{
		//	no results available
		throw uno::RuntimeException();
	}

	long nColCount = pColResRoot->GetSize(pResData->GetColStartMeasure());
	long nRowCount = pRowResRoot->GetSize(pResData->GetRowStartMeasure());

	//	allocate full sequence
	//!	leave out empty rows???

	uno::Sequence< uno::Sequence<sheet::DataResult> > aSeq( nRowCount );
	uno::Sequence<sheet::DataResult>* pRowAry = aSeq.getArray();
	for (long nRow = 0; nRow < nRowCount; nRow++)
	{
		uno::Sequence<sheet::DataResult> aColSeq( nColCount );
		//	use default values of DataResult
		pRowAry[nRow] = aColSeq;
	}

	long nSeqRow = 0;
	pRowResRoot->FillDataResults( pColResRoot, aSeq, nSeqRow, pResData->GetRowStartMeasure() );

	return aSeq;
}

void SAL_CALL ScDPSource::refresh() throw(uno::RuntimeException)
{
	disposeData();
}

void SAL_CALL ScDPSource::addRefreshListener( const uno::Reference<util::XRefreshListener >& )
												throw(uno::RuntimeException)
{
	DBG_ERROR("not implemented");	//! exception?
}

void SAL_CALL ScDPSource::removeRefreshListener( const uno::Reference<util::XRefreshListener >& )
												throw(uno::RuntimeException)
{
	DBG_ERROR("not implemented");	//! exception?
}

Sequence< Sequence<Any> > SAL_CALL ScDPSource::getDrillDownData(const Sequence<sheet::DataPilotFieldFilter>& aFilters)
    throw (uno::RuntimeException)
{
    long nColumnCount = GetData()->GetColumnCount();

    typedef hash_map<String, long, ScStringHashCode> FieldNameMapType;
    FieldNameMapType aFieldNames;
    for (long i = 0; i < nColumnCount; ++i)
    {
        aFieldNames.insert(
            FieldNameMapType::value_type(GetData()->getDimensionName(i), i));
    }

    // collect ScDPItemData for each filtered column
    vector<ScDPCacheTable::Criterion> aFilterCriteria;
    sal_Int32 nFilterCount = aFilters.getLength();
    for (sal_Int32 i = 0; i < nFilterCount; ++i)
    {
        const sheet::DataPilotFieldFilter& rFilter = aFilters[i];
        String aFieldName( rFilter.FieldName );
        for (long nCol = 0; nCol < nColumnCount; ++nCol)
        {
            if ( aFieldName == pData->getDimensionName(nCol) )
            {
                ScDPDimension* pDim = GetDimensionsObject()->getByIndex( nCol );
                ScDPMembers* pMembers = pDim->GetHierarchiesObject()->getByIndex(0)->
                                        GetLevelsObject()->getByIndex(0)->GetMembersObject();
                sal_Int32 nIndex = pMembers->GetIndexFromName( rFilter.MatchValue );
                if ( nIndex >= 0 )
                {
                    ScDPItemData aItem;
                    pMembers->getByIndex(nIndex)->FillItemData( aItem );
                    aFilterCriteria.push_back( ScDPCacheTable::Criterion() );
                    aFilterCriteria.back().mnFieldIndex = nCol;
                    aFilterCriteria.back().mpFilter.reset(
                        new ScDPCacheTable::SingleFilter(aItem.GetString()/*rSharedString, nMatchStrId*/, aItem.GetValue(), aItem.IsValue()) );
                }
            }
        }
    }

    // Take into account the visibilities of field members.
    ScDPResultVisibilityData aResVisData(/*rSharedString, */this);
    pRowResRoot->FillVisibilityData(aResVisData);
    pColResRoot->FillVisibilityData(aResVisData);
    aResVisData.fillFieldFilters(aFilterCriteria);

    Sequence< Sequence<Any> > aTabData;
    hash_set<sal_Int32> aCatDims;
    GetCategoryDimensionIndices(aCatDims);
    pData->GetDrillDownData(aFilterCriteria, aCatDims, aTabData);
    return aTabData;
}

String ScDPSource::getDataDescription()
{
	CreateRes_Impl();		// create pResData

	String aRet;
	if ( pResData->GetMeasureCount() == 1 )
    {
        bool bTotalResult = false;
        aRet = pResData->GetMeasureString( 0, sal_True, SUBTOTAL_FUNC_NONE, bTotalResult );
    }

	//	empty for more than one measure

	return aRet;
}

sal_Bool ScDPSource::getColumnGrand() const
{
	return bColumnGrand;
}

void ScDPSource::setColumnGrand(sal_Bool bSet)
{
	bColumnGrand = bSet;
}

sal_Bool ScDPSource::getRowGrand() const
{
	return bRowGrand;
}

void ScDPSource::setRowGrand(sal_Bool bSet)
{
	bRowGrand = bSet;
}

sal_Bool ScDPSource::getIgnoreEmptyRows() const
{
	return bIgnoreEmptyRows;
}

void ScDPSource::setIgnoreEmptyRows(sal_Bool bSet)
{
	bIgnoreEmptyRows = bSet;
	pData->SetEmptyFlags( bIgnoreEmptyRows, bRepeatIfEmpty );
}

sal_Bool ScDPSource::getRepeatIfEmpty() const
{
	return bRepeatIfEmpty;
}

void ScDPSource::setRepeatIfEmpty(sal_Bool bSet)
{
	bRepeatIfEmpty = bSet;
	pData->SetEmptyFlags( bIgnoreEmptyRows, bRepeatIfEmpty );
}

void ScDPSource::validate()		//! ???
{
	CreateRes_Impl();
}

void ScDPSource::disposeData()
{
	if ( pResData )
	{
		//	reset all data...

		DELETEZ(pColResRoot);
		DELETEZ(pRowResRoot);
		DELETEZ(pResData);
		delete[] pColResults;
		delete[] pRowResults;
		pColResults = NULL;
		pRowResults = NULL;
		aColLevelList.Clear();
		aRowLevelList.Clear();
	}

	if ( pDimensions )
	{
		pDimensions->release();	// ref-counted
		pDimensions = NULL;		//	settings have to be applied (from SaveData) again!
	}
	SetDupCount( 0 );

	//!	Test ????
	nColDimCount = nRowDimCount = nDataDimCount = nPageDimCount = 0;

	pData->DisposeData();	// cached entries etc.
    bPageFiltered = false;
	bResultOverflow = sal_False;
}

long lcl_CountMinMembers(const vector<ScDPDimension*>& ppDim, const vector<ScDPLevel*>& ppLevel, long nLevels )
{
    //	Calculate the product of the member count for those consecutive levels that
    //	have the "show all" flag, one following level, and the data layout dimension.

    long nTotal = 1;
    long nDataCount = 1;
    sal_Bool bWasShowAll = sal_True;
    long nPos = nLevels;
    while ( nPos > 0 )
    {
        --nPos;

        if ( nPos+1 < nLevels && ppDim[nPos] == ppDim[nPos+1] )
        {
            DBG_ERROR("lcl_CountMinMembers: multiple levels from one dimension not implemented");
            return 0;
        }

        sal_Bool bDo = sal_False;
        if ( ppDim[nPos]->getIsDataLayoutDimension() )
        {
            //	data layout dim doesn't interfere with "show all" flags
            nDataCount = ppLevel[nPos]->GetMembersObject()->getCount();
            if ( nDataCount == 0 )
                nDataCount = 1;
        }
        else if ( bWasShowAll )     // "show all" set for all following levels?
        {
            bDo = sal_True;
            if ( !ppLevel[nPos]->getShowEmpty() )
            {
                //	this level is counted, following ones are not
                bWasShowAll = sal_False;
            }
        }
        if ( bDo )
        {
            long nThisCount = ppLevel[nPos]->GetMembersObject()->getMinMembers();
            if ( nThisCount == 0 )
            {
                nTotal = 1;         //	empty level -> start counting from here
                                    //!	start with visible elements in this level?
            }
            else
            {
                if ( nTotal >= LONG_MAX / nThisCount )
                    return LONG_MAX;                        //	overflow
                nTotal *= nThisCount;
            }
        }
    }

    //	always include data layout dim, even after restarting
    if ( nTotal >= LONG_MAX / nDataCount )
        return LONG_MAX;                        //	overflow
    nTotal *= nDataCount;

    return nTotal;
}

long lcl_GetIndexFromName( const rtl::OUString rName, const uno::Sequence<rtl::OUString>& rElements )
{
    long nCount = rElements.getLength();
    const rtl::OUString* pArray = rElements.getConstArray();
    for (long nPos=0; nPos<nCount; nPos++)
        if (pArray[nPos] == rName)
            return nPos;

    return -1;  // not found
}

void ScDPSource::FillCalcInfo(bool bIsRow, ScDPTableData::CalcInfo& rInfo, bool &rHasAutoShow)
{
    long* nDims = bIsRow ? nRowDims : nColDims;
    long nDimCount = bIsRow ? nRowDimCount : nColDimCount;

    for (long i = 0; i < nDimCount; ++i)
    {
        ScDPDimension* pDim = GetDimensionsObject()->getByIndex( nDims[i] );
        long nHierarchy = pDim->getUsedHierarchy();
        if ( nHierarchy >= pDim->GetHierarchiesObject()->getCount() )
            nHierarchy = 0;
        ScDPLevels* pLevels = pDim->GetHierarchiesObject()->getByIndex(nHierarchy)->GetLevelsObject();
        long nCount = pLevels->getCount();

        //!	Test
        if ( pDim->getIsDataLayoutDimension() && nDataDimCount < 2 )
            nCount = 0;
        //!	Test

        for (long j = 0; j < nCount; ++j)
        {
            ScDPLevel* pLevel = pLevels->getByIndex(j);
            pLevel->EvaluateSortOrder();

            // no layout flags for column fields, only for row fields
            pLevel->SetEnableLayout( bIsRow );           

            if ( pLevel->GetAutoShow().IsEnabled )
                rHasAutoShow = sal_True;

            if (bIsRow)
            {
                rInfo.aRowLevelDims.push_back(nDims[i]);
                rInfo.aRowDims.push_back(pDim);
                rInfo.aRowLevels.push_back(pLevel);
            }
            else
            {
                rInfo.aColLevelDims.push_back(nDims[i]);
                rInfo.aColDims.push_back(pDim);
                rInfo.aColLevels.push_back(pLevel);
            }

            pLevel->GetMembersObject();                 // initialize for groups
        }
    }
}

void ScDPSource::GetCategoryDimensionIndices(hash_set<sal_Int32>& rCatDims)
{
    hash_set<sal_Int32> aCatDims;
    for (long i = 0; i < nColDimCount; ++i)
    {
        sal_Int32 nDim = static_cast<sal_Int32>(nColDims[i]);
        if (!IsDataLayoutDimension(nDim))
            aCatDims.insert(nDim);
    }

    for (long i = 0; i < nRowDimCount; ++i)
    {
        sal_Int32 nDim = static_cast<sal_Int32>(nRowDims[i]);
        if (!IsDataLayoutDimension(nDim))
            aCatDims.insert(nDim);
    }

    for (long i = 0; i < nPageDimCount; ++i)
    {
        sal_Int32 nDim = static_cast<sal_Int32>(nPageDims[i]);
        if (!IsDataLayoutDimension(nDim))
            aCatDims.insert(nDim);
    }

    rCatDims.swap(aCatDims);
}

void ScDPSource::FilterCacheTableByPageDimensions()
{
    // #i117661# Repeated calls to ScDPCacheTable::filterByPageDimension are invalid because
    // rows are only hidden, never shown again. If FilterCacheTableByPageDimensions is called
    // again, the cache table must be re-initialized. Currently, CreateRes_Impl always uses
    // a fresh cache because ScDBDocFunc::DataPilotUpdate calls InvalidateData.

    if (bPageFiltered)
    {
        DBG_ERRORFILE("tried to apply page field filters several times");

        pData->DisposeData();
        pData->CreateCacheTable();  // re-initialize the cache table
        bPageFiltered = false;
    }

    // filter table by page dimensions.
    vector<ScDPCacheTable::Criterion> aCriteria;
    for (long i = 0; i < nPageDimCount; ++i)
    {
        ScDPDimension* pDim = GetDimensionsObject()->getByIndex(nPageDims[i]);
        long nField = pDim->GetDimension();

        ScDPMembers* pMems = pDim->GetHierarchiesObject()->getByIndex(0)->
            GetLevelsObject()->getByIndex(0)->GetMembersObject();

        long nMemCount = pMems->getCount();
        ScDPCacheTable::Criterion aFilter;
        aFilter.mnFieldIndex = static_cast<sal_Int32>(nField);
        aFilter.mpFilter.reset(new ScDPCacheTable::GroupFilter(/*rSharedString*/));
        ScDPCacheTable::GroupFilter* pGrpFilter = 
            static_cast<ScDPCacheTable::GroupFilter*>(aFilter.mpFilter.get());
        for (long j = 0; j < nMemCount; ++j)
        {
            ScDPMember* pMem = pMems->getByIndex(j);
            if (pMem->getIsVisible())
            {
                ScDPItemData aData;
                pMem->FillItemData(aData);
                pGrpFilter->addMatchItem(aData.GetString(), aData.GetValue(), aData.IsValue());
            }
        }
        if (pGrpFilter->getMatchItemCount() < static_cast<size_t>(nMemCount))
            // there is at least one invisible item.  Add this filter criterion to the mix.
            aCriteria.push_back(aFilter);

        if (!pDim || !pDim->HasSelectedPage())
            continue;

        const ScDPItemData& rData = pDim->GetSelectedData();
        aCriteria.push_back(ScDPCacheTable::Criterion());
        ScDPCacheTable::Criterion& r = aCriteria.back();
        r.mnFieldIndex = static_cast<sal_Int32>(nField);
        r.mpFilter.reset(
            new ScDPCacheTable::SingleFilter(rData.GetString()/*rSharedString, nStrId*/, rData.GetValue(), rData.IsValue()));
    }
    if (!aCriteria.empty())
    {
        hash_set<sal_Int32> aCatDims;
        GetCategoryDimensionIndices(aCatDims);
        pData->FilterCacheTable(aCriteria, aCatDims);
        bPageFiltered = true;
    }
}

void ScDPSource::CreateRes_Impl()
{
	if ( !pResData )
	{
		sal_uInt16 nDataOrient = GetDataLayoutOrientation();
        if ( nDataDimCount > 1 && ( nDataOrient != sheet::DataPilotFieldOrientation_COLUMN &&
                                    nDataOrient != sheet::DataPilotFieldOrientation_ROW ) )
		{
			//	if more than one data dimension, data layout orientation must be set
			SetOrientation( pData->GetColumnCount(), sheet::DataPilotFieldOrientation_ROW );
			nDataOrient = sheet::DataPilotFieldOrientation_ROW;
		}

        // TODO: Aggreate pDataNames, pDataRefValues, nDataRefOrient, and
        // eDataFunctions into a structure and use vector instead of static
        // or pointer arrays.
		String* pDataNames = NULL;
		sheet::DataPilotFieldReference* pDataRefValues = NULL;
        ScSubTotalFunc eDataFunctions[SC_DAPI_MAXFIELDS];
		sal_uInt16 nDataRefOrient[SC_DAPI_MAXFIELDS];
		if (nDataDimCount)
		{
			pDataNames = new String[nDataDimCount];
			pDataRefValues = new sheet::DataPilotFieldReference[nDataDimCount];
		}

        ScDPTableData::CalcInfo aInfo;


		//	LateInit (initialize only those rows/children that are used) can be used unless
		//	any data dimension needs reference values from column/row dimensions
		sal_Bool bLateInit = sal_True;

        // Go through all data dimensions (i.e. fields) and build their meta data
        // so that they can be passed on to ScDPResultData instance later.
        // TODO: aggregate all of data dimension info into a structure.
		long i;
		for (i=0; i<nDataDimCount; i++)
		{
            // Get function for each data field.
			long nDimIndex = nDataDims[i];
			ScDPDimension* pDim = GetDimensionsObject()->getByIndex(nDimIndex);
			sheet::GeneralFunction eUser = (sheet::GeneralFunction)pDim->getFunction();
			if (eUser == sheet::GeneralFunction_AUTO)
			{
				//!	test for numeric data
				eUser = sheet::GeneralFunction_SUM;
			}

            // Map UNO's enum to internal enum ScSubTotalFunc.
			eDataFunctions[i] = ScDataUnoConversion::GeneralToSubTotal( eUser );

            // Get reference field/item information.
			pDataRefValues[i] = pDim->GetReferenceValue();
			nDataRefOrient[i] = sheet::DataPilotFieldOrientation_HIDDEN;	// default if not used
			sal_Int32 eRefType = pDataRefValues[i].ReferenceType;
			if ( eRefType == sheet::DataPilotFieldReferenceType::ITEM_DIFFERENCE ||
				 eRefType == sheet::DataPilotFieldReferenceType::ITEM_PERCENTAGE ||
				 eRefType == sheet::DataPilotFieldReferenceType::ITEM_PERCENTAGE_DIFFERENCE ||
				 eRefType == sheet::DataPilotFieldReferenceType::RUNNING_TOTAL )
			{
				long nColumn = lcl_GetIndexFromName( pDataRefValues[i].ReferenceField,
										GetDimensionsObject()->getElementNames() );
				if ( nColumn >= 0 )
				{
					nDataRefOrient[i] = GetOrientation( nColumn );
					//	need fully initialized results to find reference values
					//	(both in column or row dimensions), so updated values or
					//	differences to 0 can be displayed even for empty results.
					bLateInit = sal_False;
				}
			}

			pDataNames[i] = String( pDim->getName() );	//! label?

			//	asterisk is added to duplicated dimension names by ScDPSaveData::WriteToSource
			//!	modify user visible strings as in ScDPResultData::GetMeasureString instead!

			pDataNames[i].EraseTrailingChars('*');

			//!	if the name is overridden by user, a flag must be set
			//!	so the user defined name replaces the function string and field name.

			//!	the complete name (function and field) must be stored at the dimension

			long nSource = ((ScDPDimension*)pDim)->GetSourceDim();
            if (nSource >= 0)
                aInfo.aDataSrcCols.push_back(nSource);
            else
                aInfo.aDataSrcCols.push_back(nDimIndex);
		}

		pResData = new ScDPResultData( this );
		pResData->SetMeasureData( nDataDimCount, eDataFunctions, pDataRefValues, nDataRefOrient, pDataNames );
		pResData->SetDataLayoutOrientation(nDataOrient);
		pResData->SetLateInit( bLateInit );

		delete[] pDataNames;
		delete[] pDataRefValues;

        bool bHasAutoShow = false;

        ScDPInitState aInitState;

        // Page field selections restrict the members shown in related fields
        // (both in column and row fields). aInitState is filled with the page
        // field selections, they are kept across the data iterator loop.

        for (i=0; i<nPageDimCount; i++)
        {
            ScDPDimension* pDim = GetDimensionsObject()->getByIndex( nPageDims[i] );
            if ( pDim->HasSelectedPage() )
	            aInitState.AddMember( nPageDims[i], GetMemberId( nPageDims[i],  pDim->GetSelectedData() ) );
        }

		pColResRoot = new ScDPResultMember( pResData, /*NULL, NULL, NULL, */bColumnGrand );
		pRowResRoot = new ScDPResultMember( pResData, /*NULL, NULL, NULL, */bRowGrand );

        FillCalcInfo(false, aInfo, bHasAutoShow);
        long nColLevelCount = aInfo.aColLevels.size();

        pColResRoot->InitFrom( aInfo.aColDims, aInfo.aColLevels, 0, aInitState );
        pColResRoot->SetHasElements();

        FillCalcInfo(true, aInfo, bHasAutoShow);
        long nRowLevelCount = aInfo.aRowLevels.size();

        if ( nRowLevelCount > 0 )
        {
            // disable layout flags for the innermost row field (level)
            aInfo.aRowLevels[nRowLevelCount-1]->SetEnableLayout( sal_False );
        }

        pRowResRoot->InitFrom( aInfo.aRowDims, aInfo.aRowLevels, 0, aInitState );
        pRowResRoot->SetHasElements();

        // initialize members object also for all page dimensions (needed for numeric groups)
        for (i=0; i<nPageDimCount; i++)
        {
            ScDPDimension* pDim = GetDimensionsObject()->getByIndex( nPageDims[i] );
            long nHierarchy = pDim->getUsedHierarchy();
            if ( nHierarchy >= pDim->GetHierarchiesObject()->getCount() )
                nHierarchy = 0;

            ScDPLevels* pLevels = pDim->GetHierarchiesObject()->getByIndex(nHierarchy)->GetLevelsObject();
            long nCount = pLevels->getCount();
            for (long j=0; j<nCount; j++)
                pLevels->getByIndex(j)->GetMembersObject();             // initialize for groups
        }

		//	pre-check: calculate minimum number of result columns / rows from
		//	levels that have the "show all" flag set

		long nMinColMembers = lcl_CountMinMembers( aInfo.aColDims, aInfo.aColLevels, nColLevelCount );
		long nMinRowMembers = lcl_CountMinMembers( aInfo.aRowDims, aInfo.aRowLevels, nRowLevelCount );

		if ( nMinColMembers > MAXCOLCOUNT/*SC_MINCOUNT_LIMIT*/ || nMinRowMembers > SC_MINCOUNT_LIMIT )
		{
			//	resulting table is too big -> abort before calculating
			//	(this relies on late init, so no members are allocated in InitFrom above)

			bResultOverflow = sal_True;
		}
		else
		{
            FilterCacheTableByPageDimensions();

            aInfo.aPageDims.reserve(nPageDimCount);
            for (i = 0; i < nPageDimCount; ++i)
                aInfo.aPageDims.push_back(nPageDims[i]);

            aInfo.pInitState = &aInitState;
            aInfo.pColRoot   = pColResRoot;
            aInfo.pRowRoot   = pRowResRoot;
            pData->CalcResults(aInfo, false);

			pColResRoot->CheckShowEmpty();
			pRowResRoot->CheckShowEmpty();
			// ----------------------------------------------------------------
			//  With all data processed, calculate the final results:

            //  UpdateDataResults calculates all original results from the collected values,
			//  and stores them as reference values if needed.
			pRowResRoot->UpdateDataResults( pColResRoot, pResData->GetRowStartMeasure() );

			if ( bHasAutoShow )     // do the double calculation only if AutoShow is used
			{
			    //  Find the desired members and set bAutoHidden flag for the others
    			pRowResRoot->DoAutoShow( pColResRoot );

			    //  Reset all results to empty, so they can be built again with data for the
    			//  desired members only.
    			pColResRoot->ResetResults( sal_True );
    			pRowResRoot->ResetResults( sal_True );
                pData->CalcResults(aInfo, true);

    			//  Call UpdateDataResults again, with the new (limited) values.
    			pRowResRoot->UpdateDataResults( pColResRoot, pResData->GetRowStartMeasure() );
			}

			//  SortMembers does the sorting by a result dimension, using the original results,
			//  but not running totals etc.
			pRowResRoot->SortMembers( pColResRoot );

			//  UpdateRunningTotals calculates running totals along column/row dimensions,
			//  differences from other members (named or relative), and column/row percentages
			//  or index values.
			//  Running totals and relative differences need to be done using the sorted values.
			//  Column/row percentages and index values must be done after sorting, because the
			//  results may no longer be in the right order (row total for percentage of row is
			//  always 1).
			ScDPRunningTotalState aRunning( pColResRoot, pRowResRoot );
			ScDPRowTotals aTotals;
			pRowResRoot->UpdateRunningTotals( pColResRoot, pResData->GetRowStartMeasure(), aRunning, aTotals );

			// ----------------------------------------------------------------
		}
	}
}

//UNUSED2009-05 void ScDPSource::DumpState( ScDocument* pDoc, const ScAddress& rPos )
//UNUSED2009-05 {
//UNUSED2009-05     CreateRes_Impl();
//UNUSED2009-05 
//UNUSED2009-05     ScAddress aDocPos( rPos );
//UNUSED2009-05 
//UNUSED2009-05     if (pColResRoot->GetChildDimension())
//UNUSED2009-05         pColResRoot->GetChildDimension()->DumpState( NULL, pDoc, aDocPos );
//UNUSED2009-05     pRowResRoot->DumpState( pColResRoot, pDoc, aDocPos );
//UNUSED2009-05 }

void ScDPSource::FillLevelList( sal_uInt16 nOrientation, List& rList )
{
	rList.Clear();

	long nDimCount = 0;
	long* pDimIndex = NULL;
	switch (nOrientation)
	{
		case sheet::DataPilotFieldOrientation_COLUMN:
			pDimIndex = nColDims;
			nDimCount = nColDimCount;
			break;
		case sheet::DataPilotFieldOrientation_ROW:
			pDimIndex = nRowDims;
			nDimCount = nRowDimCount;
			break;
		case sheet::DataPilotFieldOrientation_DATA:
			pDimIndex = nDataDims;
			nDimCount = nDataDimCount;
			break;
		case sheet::DataPilotFieldOrientation_PAGE:
			pDimIndex = nPageDims;
			nDimCount = nPageDimCount;
			break;
		default:
			DBG_ERROR( "ScDPSource::FillLevelList: unexpected orientation" );
			break;
	}
	if (!pDimIndex)
	{
		DBG_ERROR("invalid orientation");
		return;
	}

	ScDPDimensions* pDims = GetDimensionsObject();
	for (long nDim=0; nDim<nDimCount; nDim++)
	{
		ScDPDimension* pDim = pDims->getByIndex(pDimIndex[nDim]);
		DBG_ASSERT( pDim->getOrientation() == nOrientation, "orientations are wrong" );

		ScDPHierarchies* pHiers = pDim->GetHierarchiesObject();
		long nHierarchy = pDim->getUsedHierarchy();
		if ( nHierarchy >= pHiers->getCount() )
			nHierarchy = 0;
		ScDPHierarchy* pHier = pHiers->getByIndex(nHierarchy);
		ScDPLevels* pLevels = pHier->GetLevelsObject();
		long nLevCount = pLevels->getCount();
		for (long nLev=0; nLev<nLevCount; nLev++)
		{
			ScDPLevel* pLevel = pLevels->getByIndex(nLev);
			rList.Insert( pLevel, LIST_APPEND );
		}
	}
}

void ScDPSource::FillMemberResults()
{
	if ( !pColResults && !pRowResults )
	{
		CreateRes_Impl();

		if ( bResultOverflow )		// set in CreateRes_Impl
		{
			//	no results available -> abort (leave empty)
			//	exception is thrown in ScDPSource::getResults
			return;
		}

		FillLevelList( sheet::DataPilotFieldOrientation_COLUMN, aColLevelList );
		long nColLevelCount = aColLevelList.Count();
		if (nColLevelCount)
		{
			long nColDimSize = pColResRoot->GetSize(pResData->GetColStartMeasure());
			pColResults = new uno::Sequence<sheet::MemberResult>[nColLevelCount];
			for (long i=0; i<nColLevelCount; i++)
				pColResults[i].realloc(nColDimSize);

			// ScDPResultDimension* pColResDim = pColResRoot->GetChildDimension();
			// pColResDim->FillMemberResults( pColResults, 0, pResData->GetColStartMeasure() );
			long nPos = 0;
			pColResRoot->FillMemberResults( pColResults, nPos, pResData->GetColStartMeasure(),
											sal_True, NULL, NULL );
		}

		FillLevelList( sheet::DataPilotFieldOrientation_ROW, aRowLevelList );
		long nRowLevelCount = aRowLevelList.Count();
		if (nRowLevelCount)
		{
			long nRowDimSize = pRowResRoot->GetSize(pResData->GetRowStartMeasure());
			pRowResults = new uno::Sequence<sheet::MemberResult>[nRowLevelCount];
			for (long i=0; i<nRowLevelCount; i++)
				pRowResults[i].realloc(nRowDimSize);

			// ScDPResultDimension* pRowResDim = pRowResRoot->GetChildDimension();
			// pRowResDim->FillMemberResults( pRowResults, 0, pResData->GetRowStartMeasure() );
			long nPos = 0;
			pRowResRoot->FillMemberResults( pRowResults, nPos, pResData->GetRowStartMeasure(),
											sal_True, NULL, NULL );
		}
	}
}

const uno::Sequence<sheet::MemberResult>* ScDPSource::GetMemberResults( ScDPLevel* pLevel )
{
	FillMemberResults();

	long i;
	long nColCount = aColLevelList.Count();
	for (i=0; i<nColCount; i++)
	{
		ScDPLevel* pColLevel = (ScDPLevel*)aColLevelList.GetObject(i);
		if ( pColLevel == pLevel )
			return pColResults+i;
	}
	long nRowCount = aRowLevelList.Count();
	for (i=0; i<nRowCount; i++)
	{
		ScDPLevel* pRowLevel = (ScDPLevel*)aRowLevelList.GetObject(i);
		if ( pRowLevel == pLevel )
			return pRowResults+i;
	}
	return NULL;
}

// XPropertySet

uno::Reference<beans::XPropertySetInfo> SAL_CALL ScDPSource::getPropertySetInfo()
														throw(uno::RuntimeException)
{
	ScUnoGuard aGuard;
    using beans::PropertyAttribute::READONLY;

    static SfxItemPropertyMapEntry aDPSourceMap_Impl[] =
	{
		{MAP_CHAR_LEN(SC_UNO_COLGRAND),	0,	&getBooleanCppuType(),				0, 0 },
		{MAP_CHAR_LEN(SC_UNO_DATADESC),	0,	&getCppuType((rtl::OUString*)0),	beans::PropertyAttribute::READONLY, 0 },
		{MAP_CHAR_LEN(SC_UNO_IGNOREEM),	0,	&getBooleanCppuType(),				0, 0 },		// for sheet data only
		{MAP_CHAR_LEN(SC_UNO_REPEATIF),	0,	&getBooleanCppuType(),				0, 0 },		// for sheet data only
		{MAP_CHAR_LEN(SC_UNO_ROWGRAND),	0,	&getBooleanCppuType(),				0, 0 },
        {MAP_CHAR_LEN(SC_UNO_ROWFIELDCOUNT),    0, &getCppuType(static_cast<sal_Int32*>(0)), READONLY, 0 },
        {MAP_CHAR_LEN(SC_UNO_COLUMNFIELDCOUNT), 0, &getCppuType(static_cast<sal_Int32*>(0)), READONLY, 0 },
        {MAP_CHAR_LEN(SC_UNO_DATAFIELDCOUNT),   0, &getCppuType(static_cast<sal_Int32*>(0)), READONLY, 0 },
        {MAP_CHAR_LEN(SC_UNO_GRANDTOTAL_NAME),  0, &getCppuType(static_cast<OUString*>(0)), 0, 0 },
        {0,0,0,0,0,0}
	};
	static uno::Reference<beans::XPropertySetInfo> aRef =
		new SfxItemPropertySetInfo( aDPSourceMap_Impl );
	return aRef;
}

void SAL_CALL ScDPSource::setPropertyValue( const rtl::OUString& aPropertyName, const uno::Any& aValue )
				throw(beans::UnknownPropertyException, beans::PropertyVetoException,
						lang::IllegalArgumentException, lang::WrappedTargetException,
						uno::RuntimeException)
{
	String aNameStr = aPropertyName;
	if ( aNameStr.EqualsAscii( SC_UNO_COLGRAND ) )
		setColumnGrand( lcl_GetBoolFromAny( aValue ) );
	else if ( aNameStr.EqualsAscii( SC_UNO_ROWGRAND ) )
		setRowGrand( lcl_GetBoolFromAny( aValue ) );
	else if ( aNameStr.EqualsAscii( SC_UNO_IGNOREEM ) )
		setIgnoreEmptyRows( lcl_GetBoolFromAny( aValue ) );
	else if ( aNameStr.EqualsAscii( SC_UNO_REPEATIF ) )
		setRepeatIfEmpty( lcl_GetBoolFromAny( aValue ) );
    else if (aNameStr.EqualsAscii(SC_UNO_GRANDTOTAL_NAME))
    {
        OUString aName;
        if (aValue >>= aName)
            mpGrandTotalName.reset(new OUString(aName));
    }
	else
	{
		DBG_ERROR("unknown property");
		//!	THROW( UnknownPropertyException() );
	}
}

uno::Any SAL_CALL ScDPSource::getPropertyValue( const rtl::OUString& aPropertyName )
				throw(beans::UnknownPropertyException, lang::WrappedTargetException,
						uno::RuntimeException)
{
	uno::Any aRet;
	String aNameStr = aPropertyName;
	if ( aNameStr.EqualsAscii( SC_UNO_COLGRAND ) )
		lcl_SetBoolInAny( aRet, getColumnGrand() );
	else if ( aNameStr.EqualsAscii( SC_UNO_ROWGRAND ) )
		lcl_SetBoolInAny( aRet, getRowGrand() );
	else if ( aNameStr.EqualsAscii( SC_UNO_IGNOREEM ) )
		lcl_SetBoolInAny( aRet, getIgnoreEmptyRows() );
	else if ( aNameStr.EqualsAscii( SC_UNO_REPEATIF ) )
		lcl_SetBoolInAny( aRet, getRepeatIfEmpty() );
	else if ( aNameStr.EqualsAscii( SC_UNO_DATADESC ) )				// read-only
		aRet <<= rtl::OUString( getDataDescription() );
    else if ( aNameStr.EqualsAscii( SC_UNO_ROWFIELDCOUNT ) )        // read-only
        aRet <<= static_cast<sal_Int32>(nRowDimCount);
    else if ( aNameStr.EqualsAscii( SC_UNO_COLUMNFIELDCOUNT ) )     // read-only
        aRet <<= static_cast<sal_Int32>(nColDimCount);
    else if ( aNameStr.EqualsAscii( SC_UNO_DATAFIELDCOUNT ) )       // read-only
        aRet <<= static_cast<sal_Int32>(nDataDimCount);
    else if (aNameStr.EqualsAscii(SC_UNO_GRANDTOTAL_NAME))
    {
        if (mpGrandTotalName.get())
            aRet <<= *mpGrandTotalName;
    }
	else
	{
		DBG_ERROR("unknown property");
		//!	THROW( UnknownPropertyException() );
	}
	return aRet;
}

SC_IMPL_DUMMY_PROPERTY_LISTENER( ScDPSource )

// -----------------------------------------------------------------------

ScDPDimensions::ScDPDimensions( ScDPSource* pSrc ) :
	pSource( pSrc ),
	ppDims( NULL )
{
	//!	hold pSource

	// include data layout dimension and duplicated dimensions
	nDimCount = pSource->GetData()->GetColumnCount() + 1 + pSource->GetDupCount();
}

ScDPDimensions::~ScDPDimensions()
{
	//!	release pSource

	if (ppDims)
	{
		for (long i=0; i<nDimCount; i++)
			if ( ppDims[i] )
				ppDims[i]->release();			// ref-counted
		delete[] ppDims;
	}
}

void ScDPDimensions::CountChanged()
{
	// include data layout dimension and duplicated dimensions
	long nNewCount = pSource->GetData()->GetColumnCount() + 1 + pSource->GetDupCount();
	if ( ppDims )
	{
		long i;
		long nCopy = Min( nNewCount, nDimCount );
		ScDPDimension** ppNew = new ScDPDimension*[nNewCount];

		for (i=0; i<nCopy; i++)				// copy existing dims
			ppNew[i] = ppDims[i];
		for (i=nCopy; i<nNewCount; i++)		// clear additional pointers
			ppNew[i] = NULL;
		for (i=nCopy; i<nDimCount; i++)		// delete old dims if count is decreased
			if ( ppDims[i] )
				ppDims[i]->release();		// ref-counted

		delete[] ppDims;
		ppDims = ppNew;
	}
	nDimCount = nNewCount;
}

// very simple XNameAccess implementation using getCount/getByIndex

uno::Any SAL_CALL ScDPDimensions::getByName( const rtl::OUString& aName )
			throw(container::NoSuchElementException,
					lang::WrappedTargetException, uno::RuntimeException)
{
	long nCount = getCount();
	for (long i=0; i<nCount; i++)
		if ( getByIndex(i)->getName() == aName )
		{
			uno::Reference<container::XNamed> xNamed = getByIndex(i);
			uno::Any aRet;
			aRet <<= xNamed;
			return aRet;
		}

	throw container::NoSuchElementException();
//    return uno::Any();
}

uno::Sequence<rtl::OUString> SAL_CALL ScDPDimensions::getElementNames() throw(uno::RuntimeException)
{
	long nCount = getCount();
	uno::Sequence<rtl::OUString> aSeq(nCount);
	rtl::OUString* pArr = aSeq.getArray();
	for (long i=0; i<nCount; i++)
		pArr[i] = getByIndex(i)->getName();
	return aSeq;
}

sal_Bool SAL_CALL ScDPDimensions::hasByName( const rtl::OUString& aName ) throw(uno::RuntimeException)
{
	long nCount = getCount();
	for (long i=0; i<nCount; i++)
		if ( getByIndex(i)->getName() == aName )
			return sal_True;
	return sal_False;
}

uno::Type SAL_CALL ScDPDimensions::getElementType() throw(uno::RuntimeException)
{
	return getCppuType((uno::Reference<container::XNamed>*)0);
}

sal_Bool SAL_CALL ScDPDimensions::hasElements() throw(uno::RuntimeException)
{
	return ( getCount() > 0 );
}

// end of XNameAccess implementation

long ScDPDimensions::getCount() const
{
	//	in tabular data, every column of source data is a dimension

	return nDimCount;
}

ScDPDimension* ScDPDimensions::getByIndex(long nIndex) const
{
	if ( nIndex >= 0 && nIndex < nDimCount )
	{
		if ( !ppDims )
		{
			((ScDPDimensions*)this)->ppDims = new ScDPDimension*[nDimCount];
			for (long i=0; i<nDimCount; i++)
				ppDims[i] = NULL;
		}
		if ( !ppDims[nIndex] )
		{
			ppDims[nIndex] = new ScDPDimension( pSource, nIndex );
			ppDims[nIndex]->acquire();		// ref-counted
		}

		return ppDims[nIndex];
	}

	return NULL;	//! exception?
}

// -----------------------------------------------------------------------

ScDPDimension::ScDPDimension( ScDPSource* pSrc, long nD ) :
	pSource( pSrc ),
	nDim( nD ),
	pHierarchies( NULL ),
	nUsedHier( 0 ),
	nFunction( SUBTOTAL_FUNC_SUM ),		// sum is default
    mpLayoutName(NULL),
    mpSubtotalName(NULL),
	nSourceDim( -1 ),
	bHasSelectedPage( sal_False ),
	pSelectedData( NULL ),
    mbHasHiddenMember(false)
{
	//!	hold pSource
}

ScDPDimension::~ScDPDimension()
{
	//!	release pSource

	if ( pHierarchies )
		pHierarchies->release();	// ref-counted

	delete pSelectedData;
}

ScDPHierarchies* ScDPDimension::GetHierarchiesObject()
{
	if (!pHierarchies)
	{
		pHierarchies = new ScDPHierarchies( pSource, nDim );
		pHierarchies->acquire();		// ref-counted
	}
	return pHierarchies;
}

const rtl::OUString* ScDPDimension::GetLayoutName() const
{
    return mpLayoutName.get();
}

const rtl::OUString* ScDPDimension::GetSubtotalName() const
{
    return mpSubtotalName.get();
}

uno::Reference<container::XNameAccess> SAL_CALL ScDPDimension::getHierarchies()
													throw(uno::RuntimeException)
{
	return GetHierarchiesObject();
}

::rtl::OUString SAL_CALL ScDPDimension::getName() throw(uno::RuntimeException)
{
	if (aName.Len())
		return aName;
	else
		return pSource->GetData()->getDimensionName( nDim );
}

void SAL_CALL ScDPDimension::setName( const ::rtl::OUString& rNewName ) throw(uno::RuntimeException)
{
	//	used after cloning
	aName = String( rNewName );
}

sal_uInt16 ScDPDimension::getOrientation() const
{
	return pSource->GetOrientation( nDim );
}

void ScDPDimension::setOrientation(sal_uInt16 nNew)
{
	pSource->SetOrientation( nDim, nNew );
}

long ScDPDimension::getPosition() const
{
	return pSource->GetPosition( nDim );
}

void ScDPDimension::setPosition(long /* nNew */)
{
	//!	...
}

sal_Bool ScDPDimension::getIsDataLayoutDimension() const
{
	return pSource->GetData()->getIsDataLayoutDimension( nDim );
}

sal_uInt16 ScDPDimension::getFunction() const
{
	return nFunction;
}

void ScDPDimension::setFunction(sal_uInt16 nNew)
{
	nFunction = nNew;
}

long ScDPDimension::getUsedHierarchy() const
{
	return nUsedHier;
}

void ScDPDimension::setUsedHierarchy(long /* nNew */)
{
    // #i52547# don't use the incomplete date hierarchy implementation - ignore the call
    // nUsedHier = nNew;
}

ScDPDimension* ScDPDimension::CreateCloneObject()
{
	DBG_ASSERT( nSourceDim < 0, "recursive duplicate - not implemented" );

	//!	set new name here, or temporary name ???
	String aNewName = aName;

	ScDPDimension* pNew = pSource->AddDuplicated( nDim, aNewName );

	pNew->aName = aNewName;				//! here or in source?
	pNew->nSourceDim = nDim;			//! recursive?

	return pNew;
}

uno::Reference<util::XCloneable> SAL_CALL ScDPDimension::createClone() throw(uno::RuntimeException)
{
	return CreateCloneObject();
}

sal_Bool ScDPDimension::isDuplicated() const
{
	return (nSourceDim >= 0);
}

const sheet::DataPilotFieldReference& ScDPDimension::GetReferenceValue() const
{
    return aReferenceValue;
}

const ScDPItemData& ScDPDimension::GetSelectedData()
{
	if ( !pSelectedData )
	{
		// find the named member to initialize pSelectedData from it, with name and value

		long nLevel = 0;		// same as in ScDPObject::FillPageList

		long nHierarchy = getUsedHierarchy();
		if ( nHierarchy >= GetHierarchiesObject()->getCount() )
			nHierarchy = 0;
		ScDPLevels* pLevels = GetHierarchiesObject()->getByIndex(nHierarchy)->GetLevelsObject();
		long nLevCount = pLevels->getCount();
		if ( nLevel < nLevCount )
		{
			ScDPMembers* pMembers = pLevels->getByIndex(nLevel)->GetMembersObject();

			//! merge with ScDPMembers::getByName
			long nCount = pMembers->getCount();
			for (long i=0; i<nCount && !pSelectedData; i++)
			{
				ScDPMember* pMember = pMembers->getByIndex(i);
				if ( pMember->GetNameStr() == aSelectedPage )
				{
					pSelectedData = new ScDPItemData();
					pMember->FillItemData( *pSelectedData );
				}
			}
		}

		if ( !pSelectedData )
			pSelectedData = new ScDPItemData( aSelectedPage, 0.0, sal_False );		// default - name only
	}

	return *pSelectedData;
}

//UNUSED2009-05 sal_Bool ScDPDimension::IsValidPage( const ScDPItemData& rData )
//UNUSED2009-05 {
//UNUSED2009-05     if ( bHasSelectedPage )
//UNUSED2009-05         return rData.IsCaseInsEqual( GetSelectedData() );
//UNUSED2009-05 
//UNUSED2009-05     return sal_True;        // no selection -> all data
//UNUSED2009-05 }

sal_Bool ScDPDimension::IsVisible( const ScDPItemData& rData )
{
	if( ScDPMembers* pMembers = this->GetHierarchiesObject()->getByIndex(0)->
		GetLevelsObject()->getByIndex(0)->GetMembersObject() )
	{
		for( long i = pMembers->getCount()-1; i>=0; i-- )
			if( ScDPMember *pDPMbr = pMembers->getByIndex( i ) )
				if( rData.IsCaseInsEqual( pDPMbr->GetItemData() ) && !pDPMbr->getIsVisible() )
					return sal_False;
	}

	return sal_True;
}
// XPropertySet

uno::Reference<beans::XPropertySetInfo> SAL_CALL ScDPDimension::getPropertySetInfo()
														throw(uno::RuntimeException)
{
	ScUnoGuard aGuard;

    static SfxItemPropertyMapEntry aDPDimensionMap_Impl[] =
	{
		{MAP_CHAR_LEN(SC_UNO_FILTER),	0,	&getCppuType((uno::Sequence<sheet::TableFilterField>*)0), 0, 0 },
        {MAP_CHAR_LEN(SC_UNO_FLAGS),    0,  &getCppuType((sal_Int32*)0),                beans::PropertyAttribute::READONLY, 0 },
		{MAP_CHAR_LEN(SC_UNO_FUNCTION),	0,	&getCppuType((sheet::GeneralFunction*)0),	0, 0 },
		{MAP_CHAR_LEN(SC_UNO_ISDATALA),	0,	&getBooleanCppuType(),						beans::PropertyAttribute::READONLY, 0 },
		{MAP_CHAR_LEN(SC_UNO_NUMBERFO),	0,	&getCppuType((sal_Int32*)0),				beans::PropertyAttribute::READONLY, 0 },
		{MAP_CHAR_LEN(SC_UNO_ORIENTAT),	0,	&getCppuType((sheet::DataPilotFieldOrientation*)0),	0, 0 },
		{MAP_CHAR_LEN(SC_UNO_ORIGINAL),	0,	&getCppuType((uno::Reference<container::XNamed>*)0), beans::PropertyAttribute::READONLY, 0 },
		{MAP_CHAR_LEN(SC_UNO_POSITION),	0,	&getCppuType((sal_Int32*)0),				0, 0 },
		{MAP_CHAR_LEN(SC_UNO_REFVALUE),	0,	&getCppuType((sheet::DataPilotFieldReference*)0), 0, 0 },
		{MAP_CHAR_LEN(SC_UNO_USEDHIER),	0,	&getCppuType((sal_Int32*)0),				0, 0 },
        {MAP_CHAR_LEN(SC_UNO_LAYOUTNAME), 0, &getCppuType(static_cast<rtl::OUString*>(0)), 0, 0 },
        {MAP_CHAR_LEN(SC_UNO_FIELD_SUBTOTALNAME), 0, &getCppuType(static_cast<rtl::OUString*>(0)), 0, 0 },
        {MAP_CHAR_LEN(SC_UNO_HAS_HIDDEN_MEMBER), 0, &getBooleanCppuType(), 0, 0 },
        {0,0,0,0,0,0}
	};
	static uno::Reference<beans::XPropertySetInfo> aRef =
		new SfxItemPropertySetInfo( aDPDimensionMap_Impl );
	return aRef;
}

void SAL_CALL ScDPDimension::setPropertyValue( const rtl::OUString& aPropertyName, const uno::Any& aValue )
				throw(beans::UnknownPropertyException, beans::PropertyVetoException,
						lang::IllegalArgumentException, lang::WrappedTargetException,
						uno::RuntimeException)
{
	String aNameStr = aPropertyName;
	if ( aNameStr.EqualsAscii( SC_UNO_POSITION ) )
	{
		sal_Int32 nInt = 0;
		if (aValue >>= nInt)
			setPosition( nInt );
	}
	else if ( aNameStr.EqualsAscii( SC_UNO_USEDHIER ) )
	{
		sal_Int32 nInt = 0;
		if (aValue >>= nInt)
			setUsedHierarchy( nInt );
	}
	else if ( aNameStr.EqualsAscii( SC_UNO_ORIENTAT ) )
	{
		sheet::DataPilotFieldOrientation eEnum;
		if (aValue >>= eEnum)
            setOrientation( sal::static_int_cast<sal_uInt16>(eEnum) );
	}
	else if ( aNameStr.EqualsAscii( SC_UNO_FUNCTION ) )
	{
		sheet::GeneralFunction eEnum;
		if (aValue >>= eEnum)
            setFunction( sal::static_int_cast<sal_uInt16>(eEnum) );
	}
	else if ( aNameStr.EqualsAscii( SC_UNO_REFVALUE ) )
        aValue >>= aReferenceValue;
	else if ( aNameStr.EqualsAscii( SC_UNO_FILTER ) )
	{
		sal_Bool bDone = sal_False;
		uno::Sequence<sheet::TableFilterField> aSeq;
		if (aValue >>= aSeq)
		{
			sal_Int32 nLength = aSeq.getLength();
			if ( nLength == 0 )
			{
				aSelectedPage.Erase();
				bHasSelectedPage = sal_False;
				bDone = sal_True;
			}
			else if ( nLength == 1 )
			{
				const sheet::TableFilterField& rField = aSeq[0];
				if ( rField.Field == 0 && rField.Operator == sheet::FilterOperator_EQUAL && !rField.IsNumeric )
				{
					aSelectedPage = rField.StringValue;
					bHasSelectedPage = sal_True;
					bDone = sal_True;
				}
			}
		}
		if ( !bDone )
		{
			DBG_ERROR("Filter property is not a single string");
			throw lang::IllegalArgumentException();
		}
		DELETEZ( pSelectedData );		// invalid after changing aSelectedPage
	}
    else if (aNameStr.EqualsAscii(SC_UNO_LAYOUTNAME))
    {
        OUString aTmpName;
        if (aValue >>= aTmpName)
            mpLayoutName.reset(new OUString(aTmpName));
    }
    else if (aNameStr.EqualsAscii(SC_UNO_FIELD_SUBTOTALNAME))
    {
        OUString aTmpName;
        if (aValue >>= aTmpName)
            mpSubtotalName.reset(new OUString(aTmpName));
    }
    else if (aNameStr.EqualsAscii(SC_UNO_HAS_HIDDEN_MEMBER))
        aValue >>= mbHasHiddenMember;
	else
	{
		DBG_ERROR("unknown property");
		//!	THROW( UnknownPropertyException() );
	}
}

uno::Any SAL_CALL ScDPDimension::getPropertyValue( const rtl::OUString& aPropertyName )
				throw(beans::UnknownPropertyException, lang::WrappedTargetException,
						uno::RuntimeException)
{
	uno::Any aRet;
	String aNameStr = aPropertyName;
	if ( aNameStr.EqualsAscii( SC_UNO_POSITION ) )
		aRet <<= (sal_Int32) getPosition();
	else if ( aNameStr.EqualsAscii( SC_UNO_USEDHIER ) )
		aRet <<= (sal_Int32) getUsedHierarchy();
	else if ( aNameStr.EqualsAscii( SC_UNO_ORIENTAT ) )
	{
		sheet::DataPilotFieldOrientation eVal = (sheet::DataPilotFieldOrientation)getOrientation();
		aRet <<= eVal;
	}
	else if ( aNameStr.EqualsAscii( SC_UNO_FUNCTION ) )
	{
		sheet::GeneralFunction eVal = (sheet::GeneralFunction)getFunction();
		aRet <<= eVal;
	}
	else if ( aNameStr.EqualsAscii( SC_UNO_REFVALUE ) )
		aRet <<= aReferenceValue;
	else if ( aNameStr.EqualsAscii( SC_UNO_ISDATALA ) )					// read-only properties
		lcl_SetBoolInAny( aRet, getIsDataLayoutDimension() );
	else if ( aNameStr.EqualsAscii( SC_UNO_NUMBERFO ) )
    {
        sal_Int32 nFormat = 0;
        sheet::GeneralFunction eFunc = (sheet::GeneralFunction)getFunction();
        // #i63745# don't use source format for "count"
        if ( eFunc != sheet::GeneralFunction_COUNT && eFunc != sheet::GeneralFunction_COUNTNUMS )
            nFormat = pSource->GetData()->GetNumberFormat( ( nSourceDim >= 0 ) ? nSourceDim : nDim );

		switch ( aReferenceValue.ReferenceType )
		{
		case sheet::DataPilotFieldReferenceType::ITEM_PERCENTAGE:
		case sheet::DataPilotFieldReferenceType::ITEM_PERCENTAGE_DIFFERENCE:
		case sheet::DataPilotFieldReferenceType::ROW_PERCENTAGE:
		case sheet::DataPilotFieldReferenceType::COLUMN_PERCENTAGE:
		case sheet::DataPilotFieldReferenceType::TOTAL_PERCENTAGE:
			nFormat = pSource->GetData()->GetNumberFormatByIdx( (NfIndexTableOffset)NF_PERCENT_DEC2 );
			break;
		case sheet::DataPilotFieldReferenceType::INDEX:
			nFormat = pSource->GetData()->GetNumberFormatByIdx( (NfIndexTableOffset)NF_NUMBER_SYSTEM );
			break;
		default:
			break;
		}

        aRet <<= nFormat;
    }
	else if ( aNameStr.EqualsAscii( SC_UNO_ORIGINAL ) )
	{
		uno::Reference<container::XNamed> xOriginal;
		if (nSourceDim >= 0)
			xOriginal = pSource->GetDimensionsObject()->getByIndex(nSourceDim);
		aRet <<= xOriginal;
	}
	else if ( aNameStr.EqualsAscii( SC_UNO_FILTER ) )
	{
		if ( bHasSelectedPage )
		{
			// single filter field: first field equal to selected string
			sheet::TableFilterField aField( sheet::FilterConnection_AND, 0,
					sheet::FilterOperator_EQUAL, sal_False, 0.0, aSelectedPage );
			aRet <<= uno::Sequence<sheet::TableFilterField>( &aField, 1 );
		}
		else
			aRet <<= uno::Sequence<sheet::TableFilterField>(0);
	}
    else if (aNameStr.EqualsAscii(SC_UNO_LAYOUTNAME))
        aRet <<= mpLayoutName.get() ? *mpLayoutName : OUString::createFromAscii("");
    else if (aNameStr.EqualsAscii(SC_UNO_FIELD_SUBTOTALNAME))
        aRet <<= mpSubtotalName.get() ? *mpSubtotalName : OUString::createFromAscii("");
    else if (aNameStr.EqualsAscii(SC_UNO_HAS_HIDDEN_MEMBER))
        aRet <<= mbHasHiddenMember;
    else if (aNameStr.EqualsAscii(SC_UNO_FLAGS))
    {
        sal_Int32 nFlags = 0;       // tabular data: all orientations are possible
        aRet <<= nFlags;
    }
	else
	{
		DBG_ERROR("unknown property");
		//!	THROW( UnknownPropertyException() );
	}
	return aRet;
}

SC_IMPL_DUMMY_PROPERTY_LISTENER( ScDPDimension )

// -----------------------------------------------------------------------

ScDPHierarchies::ScDPHierarchies( ScDPSource* pSrc, long nD ) :
	pSource( pSrc ),
	nDim( nD ),
	ppHiers( NULL )
{
	//!	hold pSource

#if 0
	//	date columns have 3 hierarchies (flat/quarter/week), other columns only one
	long nSrcDim = pSource->GetSourceDim( nDim );
	if ( pSource->IsDateDimension( nSrcDim ) )
		nHierCount = SC_DAPI_DATE_HIERARCHIES;
	else
		nHierCount = 1;
#endif

    // #i52547# don't offer the incomplete date hierarchy implementation
    nHierCount = 1;
}

ScDPHierarchies::~ScDPHierarchies()
{
	//!	release pSource

	if (ppHiers)
	{
		for (long i=0; i<nHierCount; i++)
			if ( ppHiers[i] )
				ppHiers[i]->release();		// ref-counted
		delete[] ppHiers;
	}
}

// very simple XNameAccess implementation using getCount/getByIndex

uno::Any SAL_CALL ScDPHierarchies::getByName( const rtl::OUString& aName )
			throw(container::NoSuchElementException,
					lang::WrappedTargetException, uno::RuntimeException)
{
	long nCount = getCount();
	for (long i=0; i<nCount; i++)
		if ( getByIndex(i)->getName() == aName )
		{
			uno::Reference<container::XNamed> xNamed = getByIndex(i);
			uno::Any aRet;
			aRet <<= xNamed;
			return aRet;
		}

	throw container::NoSuchElementException();
//    return uno::Any();
}

uno::Sequence<rtl::OUString> SAL_CALL ScDPHierarchies::getElementNames() throw(uno::RuntimeException)
{
	long nCount = getCount();
	uno::Sequence<rtl::OUString> aSeq(nCount);
	rtl::OUString* pArr = aSeq.getArray();
	for (long i=0; i<nCount; i++)
		pArr[i] = getByIndex(i)->getName();
	return aSeq;
}

sal_Bool SAL_CALL ScDPHierarchies::hasByName( const rtl::OUString& aName ) throw(uno::RuntimeException)
{
	long nCount = getCount();
	for (long i=0; i<nCount; i++)
		if ( getByIndex(i)->getName() == aName )
			return sal_True;
	return sal_False;
}

uno::Type SAL_CALL ScDPHierarchies::getElementType() throw(uno::RuntimeException)
{
	return getCppuType((uno::Reference<container::XNamed>*)0);
}

sal_Bool SAL_CALL ScDPHierarchies::hasElements() throw(uno::RuntimeException)
{
	return ( getCount() > 0 );
}

// end of XNameAccess implementation

long ScDPHierarchies::getCount() const
{
	return nHierCount;
}

ScDPHierarchy* ScDPHierarchies::getByIndex(long nIndex) const
{
	//	pass hierarchy index to new object in case the implementation
	//	will be extended to more than one hierarchy

	if ( nIndex >= 0 && nIndex < nHierCount )
	{
		if ( !ppHiers )
		{
			((ScDPHierarchies*)this)->ppHiers = new ScDPHierarchy*[nHierCount];
			for (long i=0; i<nHierCount; i++)
				ppHiers[i] = NULL;
		}
		if ( !ppHiers[nIndex] )
		{
			ppHiers[nIndex] = new ScDPHierarchy( pSource, nDim, nIndex );
			ppHiers[nIndex]->acquire();			// ref-counted
		}

		return ppHiers[nIndex];
	}

	return NULL;	//! exception?
}

// -----------------------------------------------------------------------

ScDPHierarchy::ScDPHierarchy( ScDPSource* pSrc, long nD, long nH ) :
	pSource( pSrc ),
	nDim( nD ),
	nHier( nH ),
	pLevels( NULL )
{
	//!	hold pSource
}

ScDPHierarchy::~ScDPHierarchy()
{
	//!	release pSource

	if (pLevels)
		pLevels->release();		// ref-counted
}

ScDPLevels* ScDPHierarchy::GetLevelsObject()
{
	if (!pLevels)
	{
		pLevels = new ScDPLevels( pSource, nDim, nHier );
		pLevels->acquire();		// ref-counted
	}
	return pLevels;
}

uno::Reference<container::XNameAccess> SAL_CALL ScDPHierarchy::getLevels()
													throw(uno::RuntimeException)
{
	return GetLevelsObject();
}

::rtl::OUString SAL_CALL ScDPHierarchy::getName() throw(uno::RuntimeException)
{
	String aRet;		//!	globstr-ID !!!!
	switch (nHier)
	{
		case SC_DAPI_HIERARCHY_FLAT:
			aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("flat"));
			break;	//! name ???????
		case SC_DAPI_HIERARCHY_QUARTER:
			aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("Quarter"));
			break;	//! name ???????
		case SC_DAPI_HIERARCHY_WEEK:
			aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("Week"));
			break;	//! name ???????
		default:
			DBG_ERROR( "ScDPHierarchy::getName: unexpected hierarchy" );
			break;
	}
	return aRet;
}

void SAL_CALL ScDPHierarchy::setName( const ::rtl::OUString& /* rNewName */ ) throw(uno::RuntimeException)
{
	DBG_ERROR("not implemented");		//! exception?
}

// -----------------------------------------------------------------------

ScDPLevels::ScDPLevels( ScDPSource* pSrc, long nD, long nH ) :
	pSource( pSrc ),
	nDim( nD ),
	nHier( nH ),
	ppLevs( NULL )
{
	//!	hold pSource

	//	text columns have only one level

	long nSrcDim = pSource->GetSourceDim( nDim );
	if ( pSource->IsDateDimension( nSrcDim ) )
	{
		switch ( nHier )
		{
			case SC_DAPI_HIERARCHY_FLAT:	nLevCount = SC_DAPI_FLAT_LEVELS;	break;
			case SC_DAPI_HIERARCHY_QUARTER:	nLevCount = SC_DAPI_QUARTER_LEVELS;	break;
			case SC_DAPI_HIERARCHY_WEEK:	nLevCount = SC_DAPI_WEEK_LEVELS;	break;
			default:
				DBG_ERROR("wrong hierarchy");
				nLevCount = 0;
		}
	}
	else
		nLevCount = 1;
}

ScDPLevels::~ScDPLevels()
{
	//!	release pSource

	if (ppLevs)
	{
		for (long i=0; i<nLevCount; i++)
			if ( ppLevs[i] )
				ppLevs[i]->release();	// ref-counted
		delete[] ppLevs;
	}
}

// very simple XNameAccess implementation using getCount/getByIndex

uno::Any SAL_CALL ScDPLevels::getByName( const rtl::OUString& aName )
			throw(container::NoSuchElementException,
					lang::WrappedTargetException, uno::RuntimeException)
{
	long nCount = getCount();
	for (long i=0; i<nCount; i++)
		if ( getByIndex(i)->getName() == aName )
		{
			uno::Reference<container::XNamed> xNamed = getByIndex(i);
			uno::Any aRet;
			aRet <<= xNamed;
			return aRet;
		}

	throw container::NoSuchElementException();
//    return uno::Any();
}

uno::Sequence<rtl::OUString> SAL_CALL ScDPLevels::getElementNames() throw(uno::RuntimeException)
{
	long nCount = getCount();
	uno::Sequence<rtl::OUString> aSeq(nCount);
	rtl::OUString* pArr = aSeq.getArray();
	for (long i=0; i<nCount; i++)
		pArr[i] = getByIndex(i)->getName();
	return aSeq;
}

sal_Bool SAL_CALL ScDPLevels::hasByName( const rtl::OUString& aName ) throw(uno::RuntimeException)
{
	long nCount = getCount();
	for (long i=0; i<nCount; i++)
		if ( getByIndex(i)->getName() == aName )
			return sal_True;
	return sal_False;
}

uno::Type SAL_CALL ScDPLevels::getElementType() throw(uno::RuntimeException)
{
	return getCppuType((uno::Reference<container::XNamed>*)0);
}

sal_Bool SAL_CALL ScDPLevels::hasElements() throw(uno::RuntimeException)
{
	return ( getCount() > 0 );
}

// end of XNameAccess implementation

long ScDPLevels::getCount() const
{
	return nLevCount;
}

ScDPLevel* ScDPLevels::getByIndex(long nIndex) const
{
	if ( nIndex >= 0 && nIndex < nLevCount )
	{
		if ( !ppLevs )
		{
			((ScDPLevels*)this)->ppLevs = new ScDPLevel*[nLevCount];
			for (long i=0; i<nLevCount; i++)
				ppLevs[i] = NULL;
		}
		if ( !ppLevs[nIndex] )
		{
			ppLevs[nIndex] = new ScDPLevel( pSource, nDim, nHier, nIndex );
			ppLevs[nIndex]->acquire();		// ref-counted
		}

		return ppLevs[nIndex];
	}

	return NULL;	//! exception?
}

// -----------------------------------------------------------------------

class ScDPGlobalMembersOrder
{
    ScDPLevel&  rLevel;
    sal_Bool        bAscending;

public:
            ScDPGlobalMembersOrder( ScDPLevel& rLev, sal_Bool bAsc ) :
                rLevel(rLev),
                bAscending(bAsc)
            {}
            ~ScDPGlobalMembersOrder() {}

    sal_Bool operator()( sal_Int32 nIndex1, sal_Int32 nIndex2 ) const;
};

sal_Bool ScDPGlobalMembersOrder::operator()( sal_Int32 nIndex1, sal_Int32 nIndex2 ) const
{
    sal_Int32 nCompare = 0;
    // seems that some ::std::sort() implementations pass the same index twice
    if( nIndex1 != nIndex2 )
    {
        ScDPMembers* pMembers = rLevel.GetMembersObject();
        ScDPMember* pMember1 = pMembers->getByIndex(nIndex1);
        ScDPMember* pMember2 = pMembers->getByIndex(nIndex2);
        nCompare = pMember1->Compare( *pMember2 );
    }
    return bAscending ? (nCompare < 0) : (nCompare > 0);
}

// -----------------------------------------------------------------------

ScDPLevel::ScDPLevel( ScDPSource* pSrc, long nD, long nH, long nL ) :
	pSource( pSrc ),
	nDim( nD ),
	nHier( nH ),
	nLev( nL ),
	pMembers( NULL ),
	bShowEmpty( sal_False ),
    aSortInfo( EMPTY_STRING, sal_True, sheet::DataPilotFieldSortMode::NAME ),   // default: sort by name
	nSortMeasure( 0 ),
	nAutoMeasure( 0 ),
	bEnableLayout( sal_False )
{
	//!	hold pSource
	//	aSubTotals is empty
}

ScDPLevel::~ScDPLevel()
{
	//!	release pSource

	if ( pMembers )
		pMembers->release();	// ref-counted
}

void ScDPLevel::EvaluateSortOrder()
{
    switch (aSortInfo.Mode)
    {
        case sheet::DataPilotFieldSortMode::DATA:
            {
                // find index of measure (index among data dimensions)

                String aDataFieldName = aSortInfo.Field;
                long nMeasureCount = pSource->GetDataDimensionCount();
                for (long nMeasure=0; nMeasure<nMeasureCount; nMeasure++)
                {
                    if ( pSource->GetDataDimName(nMeasure) == aDataFieldName )
                    {
                        nSortMeasure = nMeasure;
                        break;
                    }
                }

                //! error if not found?
            }
            break;
        case sheet::DataPilotFieldSortMode::MANUAL:
        case sheet::DataPilotFieldSortMode::NAME:
            {
                ScDPMembers* pLocalMembers = GetMembersObject();
                long nCount = pLocalMembers->getCount();

//                DBG_ASSERT( aGlobalOrder.empty(), "sort twice?" );
                aGlobalOrder.resize( nCount );
                for (long nPos=0; nPos<nCount; nPos++)
                    aGlobalOrder[nPos] = nPos;

                // allow manual or name (manual is always ascending)
                sal_Bool bAscending = ( aSortInfo.Mode == sheet::DataPilotFieldSortMode::MANUAL || aSortInfo.IsAscending );
                ScDPGlobalMembersOrder aComp( *this, bAscending );
                ::std::sort( aGlobalOrder.begin(), aGlobalOrder.end(), aComp );
            }
            break;
    }

    if ( aAutoShowInfo.IsEnabled )
    {
        // find index of measure (index among data dimensions)

        String aDataFieldName = aAutoShowInfo.DataField;
        long nMeasureCount = pSource->GetDataDimensionCount();
        for (long nMeasure=0; nMeasure<nMeasureCount; nMeasure++)
        {
            if ( pSource->GetDataDimName(nMeasure) == aDataFieldName )
            {
                nAutoMeasure = nMeasure;
                break;
            }
        }

        //! error if not found?
    }
}

void ScDPLevel::SetEnableLayout( sal_Bool bSet )
{
    bEnableLayout = bSet;
}

ScDPMembers* ScDPLevel::GetMembersObject()
{
	if (!pMembers)
	{
		pMembers = new ScDPMembers( pSource, nDim, nHier, nLev );
		pMembers->acquire();	// ref-counted
	}
	return pMembers;
}

uno::Reference<container::XNameAccess> SAL_CALL ScDPLevel::getMembers() throw(uno::RuntimeException)
{
	return GetMembersObject();
}

uno::Sequence<sheet::MemberResult> SAL_CALL ScDPLevel::getResults() throw(uno::RuntimeException)
{
	const uno::Sequence<sheet::MemberResult>* pRes = pSource->GetMemberResults( this );
	if (pRes)
		return *pRes;

	return uno::Sequence<sheet::MemberResult>(0);		//! Error?
}

::rtl::OUString SAL_CALL ScDPLevel::getName() throw(uno::RuntimeException)
{
	long nSrcDim = pSource->GetSourceDim( nDim );
	if ( pSource->IsDateDimension( nSrcDim ) )
	{
		String aRet;		//!	globstr-ID !!!!

		if ( nHier == SC_DAPI_HIERARCHY_QUARTER )
		{
			switch ( nLev )
			{
				case SC_DAPI_LEVEL_YEAR:
					aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("Year"));
					break;
				case SC_DAPI_LEVEL_QUARTER:
					aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("Quarter"));
					break;
				case SC_DAPI_LEVEL_MONTH:
					aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("Month"));
					break;
				case SC_DAPI_LEVEL_DAY:
					aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("Day"));
					break;
				default:
					DBG_ERROR( "ScDPLevel::getName: unexpected level" );
					break;
			}
		}
		else if ( nHier == SC_DAPI_HIERARCHY_WEEK )
		{
			switch ( nLev )
			{
				case SC_DAPI_LEVEL_YEAR:
					aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("Year"));
					break;
				case SC_DAPI_LEVEL_WEEK:
					aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("Week"));
					break;
				case SC_DAPI_LEVEL_WEEKDAY:
					aRet = String::CreateFromAscii(RTL_CONSTASCII_STRINGPARAM("Weekday"));
					break;
				default:
					DBG_ERROR( "ScDPLevel::getName: unexpected level" );
					break;
			}
		}
		if (aRet.Len())
			return aRet;
	}

    ScDPDimension* pDim = pSource->GetDimensionsObject()->getByIndex(nSrcDim);
    if (!pDim)
        return rtl::OUString();

    return pDim->getName();
}

void SAL_CALL ScDPLevel::setName( const ::rtl::OUString& /* rNewName */ ) throw(uno::RuntimeException)
{
	DBG_ERROR("not implemented");		//! exception?
}

uno::Sequence<sheet::GeneralFunction> ScDPLevel::getSubTotals() const
{
	//!	separate functions for settings and evaluation?

	long nSrcDim = pSource->GetSourceDim( nDim );
	if ( !pSource->SubTotalAllowed( nSrcDim ) )
		return uno::Sequence<sheet::GeneralFunction>(0);

	return aSubTotals;
}

void ScDPLevel::setSubTotals(const uno::Sequence<sheet::GeneralFunction>& rNew)
{
	aSubTotals = rNew;
	//!	set "manual change" flag?
}

sal_Bool ScDPLevel::getShowEmpty() const
{
	return bShowEmpty;
}

void ScDPLevel::setShowEmpty(sal_Bool bSet)
{
	bShowEmpty = bSet;
}

// XPropertySet

uno::Reference<beans::XPropertySetInfo> SAL_CALL ScDPLevel::getPropertySetInfo()
														throw(uno::RuntimeException)
{
	ScUnoGuard aGuard;

    static SfxItemPropertyMapEntry aDPLevelMap_Impl[] =
	{
		//! change type of AutoShow/Layout/Sorting to API struct when available
		{MAP_CHAR_LEN(SC_UNO_AUTOSHOW),	0,	&getCppuType((sheet::DataPilotFieldAutoShowInfo*)0),     0, 0 },
		{MAP_CHAR_LEN(SC_UNO_LAYOUT),	0,	&getCppuType((sheet::DataPilotFieldLayoutInfo*)0),       0, 0 },
		{MAP_CHAR_LEN(SC_UNO_SHOWEMPT),	0,	&getBooleanCppuType(),									 0, 0 },
		{MAP_CHAR_LEN(SC_UNO_SORTING),	0,	&getCppuType((sheet::DataPilotFieldSortInfo*)0),         0, 0 },
		{MAP_CHAR_LEN(SC_UNO_SUBTOTAL),	0,	&getCppuType((uno::Sequence<sheet::GeneralFunction>*)0), 0, 0 },
        {0,0,0,0,0,0}
	};
	static uno::Reference<beans::XPropertySetInfo> aRef =
		new SfxItemPropertySetInfo( aDPLevelMap_Impl );
	return aRef;
}

void SAL_CALL ScDPLevel::setPropertyValue( const rtl::OUString& aPropertyName, const uno::Any& aValue )
				throw(beans::UnknownPropertyException, beans::PropertyVetoException,
						lang::IllegalArgumentException, lang::WrappedTargetException,
						uno::RuntimeException)
{
	String aNameStr = aPropertyName;
	if ( aNameStr.EqualsAscii( SC_UNO_SHOWEMPT ) )
		setShowEmpty( lcl_GetBoolFromAny( aValue ) );
	else if ( aNameStr.EqualsAscii( SC_UNO_SUBTOTAL ) )
	{
		uno::Sequence<sheet::GeneralFunction> aSeq;
		if ( aValue >>= aSeq )
			setSubTotals( aSeq );
	}
	else if ( aNameStr.EqualsAscii( SC_UNO_SORTING ) )
        aValue >>= aSortInfo;
	else if ( aNameStr.EqualsAscii( SC_UNO_AUTOSHOW ) )
	    aValue >>= aAutoShowInfo;
	else if ( aNameStr.EqualsAscii( SC_UNO_LAYOUT ) )
	    aValue >>= aLayoutInfo;
	else
	{
		DBG_ERROR("unknown property");
		//!	THROW( UnknownPropertyException() );
	}
}

uno::Any SAL_CALL ScDPLevel::getPropertyValue( const rtl::OUString& aPropertyName )
				throw(beans::UnknownPropertyException, lang::WrappedTargetException,
						uno::RuntimeException)
{
	uno::Any aRet;
	String aNameStr = aPropertyName;
	if ( aNameStr.EqualsAscii( SC_UNO_SHOWEMPT ) )
		lcl_SetBoolInAny( aRet, getShowEmpty() );
	else if ( aNameStr.EqualsAscii( SC_UNO_SUBTOTAL ) )
	{
		uno::Sequence<sheet::GeneralFunction> aSeq = getSubTotals();		//! avoid extra copy?
		aRet <<= aSeq;
	}
	else if ( aNameStr.EqualsAscii( SC_UNO_SORTING ) )
        aRet <<= aSortInfo;
	else if ( aNameStr.EqualsAscii( SC_UNO_AUTOSHOW ) )
	    aRet <<= aAutoShowInfo;
	else if ( aNameStr.EqualsAscii( SC_UNO_LAYOUT ) )
	    aRet <<= aLayoutInfo;
    else if (aNameStr.EqualsAscii(SC_UNO_LAYOUTNAME))
    {
        // read only property
        long nSrcDim = pSource->GetSourceDim(nDim);
        ScDPDimension* pDim = pSource->GetDimensionsObject()->getByIndex(nSrcDim);
        if (!pDim)
            return aRet;

        const OUString* pLayoutName = pDim->GetLayoutName();
        if (!pLayoutName)
            return aRet;

        aRet <<= *pLayoutName;
    }
	else
	{
		DBG_ERROR("unknown property");
		//!	THROW( UnknownPropertyException() );
	}
	return aRet;
}

SC_IMPL_DUMMY_PROPERTY_LISTENER( ScDPLevel )

// -----------------------------------------------------------------------

ScDPMembers::ScDPMembers( ScDPSource* pSrc, long nD, long nH, long nL ) :
	pSource( pSrc ),
	nDim( nD ),
	nHier( nH ),
	nLev( nL ),
	ppMbrs( NULL )
{
	//!	hold pSource

	long nSrcDim = pSource->GetSourceDim( nDim );
	if ( pSource->IsDataLayoutDimension(nSrcDim) )
		nMbrCount = pSource->GetDataDimensionCount();
	else if ( nHier != SC_DAPI_HIERARCHY_FLAT && pSource->IsDateDimension( nSrcDim ) )
	{
		nMbrCount = 0;
		if ( nHier == SC_DAPI_HIERARCHY_QUARTER )
		{
			switch (nLev)
			{
				case SC_DAPI_LEVEL_YEAR:
                    {
                        // Wang Xu Ming - DataPilot migration
                        const ScDPItemData* pLastNumData = NULL;
                        for ( SCROW n = 0 ;n <GetSrcItemsCount() ; n-- )
                        {
                            const ScDPItemData* pData  = GetSrcItemDataByIndex( n );
                            if ( pData && pData->HasStringData() )
                                break;
                            else
                                pLastNumData = pData;
                        }
                        // End Comments

                        if ( pLastNumData )
						{
                            const ScDPItemData*  pFirstData = GetSrcItemDataByIndex( 0 );
                            double fFirstVal = pFirstData->GetValue();
							double fLastVal = pLastNumData->GetValue();

							long nFirstYear = pSource->GetData()->GetDatePart(
										(long)::rtl::math::approxFloor( fFirstVal ),
										nHier, nLev );
							long nLastYear = pSource->GetData()->GetDatePart(
										(long)::rtl::math::approxFloor( fLastVal ),
										nHier, nLev );

							nMbrCount = nLastYear + 1 - nFirstYear;
						}
						else
							nMbrCount = 0;		// no values
					}
					break;
				case SC_DAPI_LEVEL_QUARTER:	nMbrCount = 4;	break;
				case SC_DAPI_LEVEL_MONTH:	nMbrCount = 12;	break;
				case SC_DAPI_LEVEL_DAY:		nMbrCount = 31;	break;
				default:
					DBG_ERROR( "ScDPMembers::ScDPMembers: unexpected level" );
					break;
			}
		}
		else if ( nHier == SC_DAPI_HIERARCHY_WEEK )
		{
			switch (nLev)
			{
				case SC_DAPI_LEVEL_YEAR:	nMbrCount = 1;	break;		//! get years from source
				case SC_DAPI_LEVEL_WEEK:	nMbrCount = 53;	break;
				case SC_DAPI_LEVEL_WEEKDAY:	nMbrCount = 7;	break;
				default:
					DBG_ERROR( "ScDPMembers::ScDPMembers: unexpected level" );
					break;
			}
		}
	}
	else
		nMbrCount = pSource->GetData()->GetMembersCount( nSrcDim );
}

ScDPMembers::~ScDPMembers()
{
	//!	release pSource

	if (ppMbrs)
	{
		for (long i=0; i<nMbrCount; i++)
			if ( ppMbrs[i] )
				ppMbrs[i]->release();	// ref-counted
		delete[] ppMbrs;
	}
}

// XNameAccess implementation using getCount/getByIndex

sal_Int32 ScDPMembers::GetIndexFromName( const ::rtl::OUString& rName ) const
{
    if ( aHashMap.empty() )
    {
        // store the index for each name

        sal_Int32 nCount = getCount();
        for (sal_Int32 i=0; i<nCount; i++)
            aHashMap[ getByIndex(i)->getName() ] = i;
    }

    ScDPMembersHashMap::const_iterator aIter = aHashMap.find( rName );
    if ( aIter != aHashMap.end() )
        return aIter->second;           // found index
    else
        return -1;                      // not found
}

uno::Any SAL_CALL ScDPMembers::getByName( const rtl::OUString& aName )
			throw(container::NoSuchElementException,
					lang::WrappedTargetException, uno::RuntimeException)
{
    sal_Int32 nIndex = GetIndexFromName( aName );
    if ( nIndex >= 0 )
    {
        uno::Reference<container::XNamed> xNamed = getByIndex(nIndex);
        uno::Any aRet;
        aRet <<= xNamed;
        return aRet;
    }

	throw container::NoSuchElementException();
//    return uno::Any();
}

uno::Sequence<rtl::OUString> SAL_CALL ScDPMembers::getElementNames() throw(uno::RuntimeException)
{
    // Return list of names in sorted order,
    // so it's displayed in that order in the field options dialog.
    // Sorting is done at the level object (parent of this).

    ScDPLevel* pLevel = pSource->GetDimensionsObject()->getByIndex(nDim)->
        GetHierarchiesObject()->getByIndex(nHier)->GetLevelsObject()->getByIndex(nLev);
    pLevel->EvaluateSortOrder();
    const std::vector<sal_Int32>& rGlobalOrder = pLevel->GetGlobalOrder();
    bool bSort = !rGlobalOrder.empty();

	long nCount = getCount();
	uno::Sequence<rtl::OUString> aSeq(nCount);
	rtl::OUString* pArr = aSeq.getArray();
	for (long i=0; i<nCount; i++)
        pArr[i] = getByIndex(bSort ? rGlobalOrder[i] : i)->getName();
	return aSeq;
}

sal_Bool SAL_CALL ScDPMembers::hasByName( const rtl::OUString& aName ) throw(uno::RuntimeException)
{
    return ( GetIndexFromName( aName ) >= 0 );
}

uno::Type SAL_CALL ScDPMembers::getElementType() throw(uno::RuntimeException)
{
	return getCppuType((uno::Reference<container::XNamed>*)0);
}

sal_Bool SAL_CALL ScDPMembers::hasElements() throw(uno::RuntimeException)
{
	return ( getCount() > 0 );
}

// end of XNameAccess implementation

long ScDPMembers::getCount() const
{
	return nMbrCount;
}

long ScDPMembers::getMinMembers() const
{
	// used in lcl_CountMinMembers

	long nVisCount = 0;
	if ( ppMbrs )
	{
		for (long i=0; i<nMbrCount; i++)
		{
			//	count only visible with details (default is true for both)
			const ScDPMember* pMbr = ppMbrs[i];
			if ( !pMbr || ( pMbr->getIsVisible() && pMbr->getShowDetails() ) )
				++nVisCount;
		}
	}
	else
		nVisCount = nMbrCount;		// default for all

	return nVisCount;
}

ScDPMember* ScDPMembers::getByIndex(long nIndex) const
{
	//	result of GetColumnEntries must not change between ScDPMembers ctor
	//	and all calls to getByIndex

	if ( nIndex >= 0 && nIndex < nMbrCount )
	{
		if ( !ppMbrs )
		{
			((ScDPMembers*)this)->ppMbrs = new ScDPMember*[nMbrCount];
			for (long i=0; i<nMbrCount; i++)
				ppMbrs[i] = NULL;
		}
		if ( !ppMbrs[nIndex] )
		{
			ScDPMember* pNew;
			long nSrcDim = pSource->GetSourceDim( nDim );
			if ( pSource->IsDataLayoutDimension(nSrcDim) )
			{
				// empty name (never shown, not used for lookup)
                pNew = new ScDPMember( pSource, nDim, nHier, nLev, 0 );
			}
			else if ( nHier != SC_DAPI_HIERARCHY_FLAT && pSource->IsDateDimension( nSrcDim ) )
			{
				long nVal = 0;
				String aName;

				if ( nLev == SC_DAPI_LEVEL_YEAR )	// YEAR is in both hierarchies
				{
					//!	cache year range here!
					
					// Wang Xu Ming - DataPilot migration
					double fFirstVal = pSource->GetData()->GetMemberByIndex( nSrcDim, 0 )->GetValue();
					long nFirstYear = pSource->GetData()->GetDatePart(
										(long)::rtl::math::approxFloor( fFirstVal ),
										nHier, nLev );

					// End Comments
					nVal = nFirstYear + nIndex;
				}
				else if ( nHier == SC_DAPI_HIERARCHY_WEEK && nLev == SC_DAPI_LEVEL_WEEKDAY )
				{
					nVal = nIndex;				// DayOfWeek is 0-based
					aName = ScGlobal::GetCalendar()->getDisplayName(
						::com::sun::star::i18n::CalendarDisplayIndex::DAY,
                        sal::static_int_cast<sal_Int16>(nVal), 0 );
				}
				else if ( nHier == SC_DAPI_HIERARCHY_QUARTER && nLev == SC_DAPI_LEVEL_MONTH )
				{
					nVal = nIndex;				// Month is 0-based
					aName = ScGlobal::GetCalendar()->getDisplayName(
						::com::sun::star::i18n::CalendarDisplayIndex::MONTH,
                        sal::static_int_cast<sal_Int16>(nVal), 0 );
				}
				else
					nVal = nIndex + 1;			// Quarter, Day, Week are 1-based

				if ( !aName.Len() )
					aName = String::CreateFromInt32(nVal);
                   
                		    ScDPItemData  rData( aName, nVal, sal_True, 0 ) ;
				    pNew = new ScDPMember( pSource, nDim, nHier, nLev, pSource->GetCache()->GetAdditionalItemID(rData));
			}
			else
			{
			         const std::vector< SCROW >& memberIndexs = pSource->GetData()->GetColumnEntries( nSrcDim );
				    pNew = new ScDPMember( pSource, nDim, nHier, nLev, memberIndexs[nIndex]   );
			}
			pNew->acquire();			// ref-counted
			ppMbrs[nIndex] = pNew;
		}

		DBG_ASSERT( ppMbrs[nIndex] ," member is not initialized " );

		return ppMbrs[nIndex];
	}

	return NULL;	//! exception?
}

// -----------------------------------------------------------------------

ScDPMember::ScDPMember( ScDPSource* pSrc, long nD, long nH, long nL,
						SCROW nIndex /*const String& rN, double fV, sal_Bool bHV*/ ) :
	pSource( pSrc ),
	nDim( nD ),
	nHier( nH ),
	nLev( nL ),
    mnDataId( nIndex ),
    mpLayoutName(NULL),
    nPosition( -1 ),
	bVisible( sal_True ),
	bShowDet( sal_True )
{
	//!	hold pSource
}

ScDPMember::~ScDPMember()
{
	//!	release pSource
}

sal_Bool ScDPMember::IsNamedItem( const ScDPItemData& r ) const
{
	long nSrcDim = pSource->GetSourceDim( nDim );
	if ( nHier != SC_DAPI_HIERARCHY_FLAT && pSource->IsDateDimension( nSrcDim ) && r.IsValue() )
	{
		long nComp = pSource->GetData()->GetDatePart(
										(long)::rtl::math::approxFloor( r.GetValue() ),
										nHier, nLev );

		//	fValue is converted from integer, so simple comparison works
		return nComp == GetItemData().GetValue();
	}

	return r.IsCaseInsEqual( GetItemData() );
}

sal_Bool ScDPMember::IsNamedItem( SCROW    nIndex ) const
{
	long nSrcDim = pSource->GetSourceDim( nDim );
	if ( nHier != SC_DAPI_HIERARCHY_FLAT && pSource->IsDateDimension( nSrcDim ) )
	{ 
        const ScDPItemData* pData =  pSource->GetCache()->GetItemDataById( (SCCOL) nSrcDim, nIndex );
        if (  pData->IsValue()  )
        {
            long nComp = pSource->GetData()->GetDatePart(
                (long)::rtl::math::approxFloor( pData->GetValue() ),
                nHier, nLev );
            //  fValue is converted from integer, so simple comparison works
            return nComp == GetItemData().GetValue();
        }
    }

	return  nIndex == mnDataId;
}

sal_Int32 ScDPMember::Compare( const ScDPMember& rOther ) const
{
    if ( nPosition >= 0 )
    {
        if ( rOther.nPosition >= 0 )
        {
            DBG_ASSERT( nPosition != rOther.nPosition, "same position for two members" );
            return ( nPosition < rOther.nPosition ) ? -1 : 1;
        }
        else
        {
            // only this has a position - members with specified positions come before those without
            return -1;
        }
    }
    else if ( rOther.nPosition >= 0 )
    {
        // only rOther has a position
        return 1;
    }

    // no positions set - compare names
   return pSource->GetData()->Compare( pSource->GetSourceDim(nDim),mnDataId,rOther.GetItemDataId());
}

void ScDPMember::FillItemData( ScDPItemData& rData ) const
{
	//!	handle date hierarchy...

	rData = GetItemData() ;
}

const OUString* ScDPMember::GetLayoutName() const
{
    return mpLayoutName.get();
}

String ScDPMember::GetNameStr() const
{
	  return GetItemData().GetString();
}

::rtl::OUString SAL_CALL ScDPMember::getName() throw(uno::RuntimeException)
{
	  return GetItemData().GetString();
}

void SAL_CALL ScDPMember::setName( const ::rtl::OUString& /* rNewName */ ) throw(uno::RuntimeException)
{
	DBG_ERROR("not implemented");		//! exception?
}

sal_Bool ScDPMember::getIsVisible() const
{
	return bVisible;
}

void ScDPMember::setIsVisible(sal_Bool bSet)
{
	bVisible = bSet;
	//!	set "manual change" flag
}

sal_Bool ScDPMember::getShowDetails() const
{
	return bShowDet;
}

void ScDPMember::setShowDetails(sal_Bool bSet)
{
	bShowDet = bSet;
	//!	set "manual change" flag
}

sal_Int32 ScDPMember::getPosition() const
{
    return nPosition;
}

void ScDPMember::setPosition(sal_Int32 nNew)
{
    nPosition = nNew;
}

// XPropertySet

uno::Reference<beans::XPropertySetInfo> SAL_CALL ScDPMember::getPropertySetInfo()
														throw(uno::RuntimeException)
{
	ScUnoGuard aGuard;

    static SfxItemPropertyMapEntry aDPMemberMap_Impl[] =
	{
		{MAP_CHAR_LEN(SC_UNO_ISVISIBL),	0,	&getBooleanCppuType(),				0, 0 },
        {MAP_CHAR_LEN(SC_UNO_POSITION), 0,  &getCppuType((sal_Int32*)0),        0, 0 },
		{MAP_CHAR_LEN(SC_UNO_SHOWDETA),	0,	&getBooleanCppuType(),				0, 0 },
        {MAP_CHAR_LEN(SC_UNO_LAYOUTNAME), 0, &getCppuType(static_cast<rtl::OUString*>(0)), 0, 0 },
        {0,0,0,0,0,0}
	};
	static uno::Reference<beans::XPropertySetInfo> aRef =
		new SfxItemPropertySetInfo( aDPMemberMap_Impl );
	return aRef;
}

void SAL_CALL ScDPMember::setPropertyValue( const rtl::OUString& aPropertyName, const uno::Any& aValue )
				throw(beans::UnknownPropertyException, beans::PropertyVetoException,
						lang::IllegalArgumentException, lang::WrappedTargetException,
						uno::RuntimeException)
{
	String aNameStr = aPropertyName;
	if ( aNameStr.EqualsAscii( SC_UNO_ISVISIBL ) )
		setIsVisible( lcl_GetBoolFromAny( aValue ) );
	else if ( aNameStr.EqualsAscii( SC_UNO_SHOWDETA ) )
		setShowDetails( lcl_GetBoolFromAny( aValue ) );
	else if ( aNameStr.EqualsAscii( SC_UNO_POSITION ) )
	{
        sal_Int32 nInt = 0;
        if (aValue >>= nInt)
            setPosition( nInt );
	}
    else if (aNameStr.EqualsAscii(SC_UNO_LAYOUTNAME))
    {
        rtl::OUString aName;
        if (aValue >>= aName)
            mpLayoutName.reset(new rtl::OUString(aName));
    }
	else
	{
		DBG_ERROR("unknown property");
		//!	THROW( UnknownPropertyException() );
	}
}

uno::Any SAL_CALL ScDPMember::getPropertyValue( const rtl::OUString& aPropertyName )
				throw(beans::UnknownPropertyException, lang::WrappedTargetException,
						uno::RuntimeException)
{
	uno::Any aRet;
	String aNameStr = aPropertyName;
	if ( aNameStr.EqualsAscii( SC_UNO_ISVISIBL ) )
		lcl_SetBoolInAny( aRet, getIsVisible() );
	else if ( aNameStr.EqualsAscii( SC_UNO_SHOWDETA ) )
		lcl_SetBoolInAny( aRet, getShowDetails() );
    else if ( aNameStr.EqualsAscii( SC_UNO_POSITION ) )
        aRet <<= (sal_Int32) getPosition();
    else if (aNameStr.EqualsAscii(SC_UNO_LAYOUTNAME))
        aRet <<= mpLayoutName.get() ? *mpLayoutName : rtl::OUString();
	else
	{
		DBG_ERROR("unknown property");
		//!	THROW( UnknownPropertyException() );
	}
	return aRet;
}

SC_IMPL_DUMMY_PROPERTY_LISTENER( ScDPMember )


ScDPTableDataCache* ScDPSource::GetCache()
{
	DBG_ASSERT( GetData() , "empty ScDPTableData pointer");
	return ( GetData()!=NULL) ? GetData()->GetCacheTable().GetCache() : NULL ;
}

const ScDPItemData& ScDPMember::GetItemData() const
{
    return *pSource->GetItemDataById( (SCCOL)nDim, mnDataId );//ms-cache-core
}

const ScDPItemData* ScDPSource::GetItemDataById(long nDim, long nId)
{
    long nSrcDim = GetSourceDim( nDim ); 
    const ScDPItemData* pItemData = GetData()->GetMemberById(  nSrcDim,  nId );
    if ( !pItemData )
   { //todo:
   	    ScDPItemData item;
   	    nId = GetCache()->GetAdditionalItemID( item );
        pItemData = GetData()->GetMemberById(  nSrcDim,  nId );
    }
   return pItemData;
}

SCROW  ScDPSource::GetMemberId(  long  nDim, const ScDPItemData& rData )
{
	long nSrcDim = GetSourceDim( nDim );
       return  GetCache()->GetIdByItemData(  nSrcDim, rData ); 
}

const ScDPItemData* ScDPMembers::GetSrcItemDataByIndex( SCROW nIndex)
{
    const std::vector< SCROW >& memberIds = pSource->GetData()->GetColumnEntries( nDim );
    if ( nIndex >= (long )(memberIds.size()) || nIndex < 0 )
        return NULL;
    SCROW nId =  memberIds[ nIndex ];                  
    return pSource->GetItemDataById( nDim, nId );
}

 SCROW ScDPMembers::GetSrcItemsCount()
 {
    return pSource->GetData()->GetColumnEntries( nDim ).size();
 }
// End Comments


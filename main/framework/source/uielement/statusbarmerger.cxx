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
#include "precompiled_framework.hxx"

#include <uielement/statusbarmerger.hxx>

using rtl::OUString;
using com::sun::star::frame::XFrame;
using com::sun::star::uno::Reference;
using com::sun::star::beans::PropertyValue;
using com::sun::star::uno::Sequence;

namespace framework
{
namespace {

static const char MERGE_STATUSBAR_URL[]         = "URL";
static const char MERGE_STATUSBAR_TITLE[]       = "Title";
static const char MERGE_STATUSBAR_CONTEXT[]     = "Context";
static const char MERGE_STATUSBAR_ALIGN[]       = "Alignment";
static const char MERGE_STATUSBAR_AUTOSIZE[]    = "AutoSize";
static const char MERGE_STATUSBAR_OWNERDRAW[]   = "OwnerDraw";
static const char MERGE_STATUSBAR_WIDTH[]       = "Width";

static const char STATUSBAR_ALIGN_LEFT[]        = "left";
static const char STATUSBAR_ALIGN_CENTER[]      = "center";
static const char STATUSBAR_ALIGN_RIGHT[]       = "right";

static const char MERGECOMMAND_ADDAFTER[]       = "AddAfter";
static const char MERGECOMMAND_ADDBEFORE[]      = "AddBefore";
static const char MERGECOMMAND_REPLACE[]        = "Replace";
static const char MERGECOMMAND_REMOVE[]         = "Remove";

static const char MERGEFALLBACK_ADDLAST[]       = "AddLast";
static const char MERGEFALLBACK_ADDFIRST[]      = "AddFirst";
static const char MERGEFALLBACK_IGNORE[]        = "Ignore";


static void lcl_ConvertSequenceToValues(
    const Sequence< PropertyValue > &rSequence,
    AddonStatusbarItem &rItem )
{
    OUString sAlignment;
    sal_Bool bAutoSize = sal_False;
    sal_Bool bOwnerDraw = sal_False;

    PropertyValue aPropVal;
    for ( sal_Int32 i = 0; i < rSequence.getLength(); i++ )
    {
        aPropVal = rSequence[i];
        if ( aPropVal.Name.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGE_STATUSBAR_URL ) ) )
            aPropVal.Value >>= rItem.aCommandURL;
        else if ( aPropVal.Name.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGE_STATUSBAR_TITLE ) ) )
            aPropVal.Value >>= rItem.aLabel;
        else if ( aPropVal.Name.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGE_STATUSBAR_CONTEXT ) ) )
            aPropVal.Value >>= rItem.aContext;
        else if ( aPropVal.Name.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGE_STATUSBAR_ALIGN ) ) )
            aPropVal.Value >>= sAlignment;
        else if ( aPropVal.Name.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGE_STATUSBAR_AUTOSIZE ) ) )
            aPropVal.Value >>= bAutoSize;
        else if ( aPropVal.Name.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGE_STATUSBAR_OWNERDRAW ) ) )
            aPropVal.Value >>= bOwnerDraw;
        else if ( aPropVal.Name.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGE_STATUSBAR_WIDTH ) ) )
        {
            sal_Int32 aWidth = 0;
            aPropVal.Value >>= aWidth;
            rItem.nWidth = sal_uInt16( aWidth );
        }
    }

    sal_uInt16 nItemBits(0);
    if ( bAutoSize )
        nItemBits |= SIB_AUTOSIZE;
    if ( bOwnerDraw )
        nItemBits |= SIB_USERDRAW;
    if ( sAlignment.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( STATUSBAR_ALIGN_CENTER )))
        nItemBits |= SIB_CENTER;
    else if ( sAlignment.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( STATUSBAR_ALIGN_RIGHT )))
        nItemBits |= SIB_RIGHT;
    else
        // if unset, defaults to left alignment
        nItemBits |= SIB_LEFT;
    rItem.nItemBits = nItemBits;
}

static void lcl_CreateStatusbarItem( StatusBar* pStatusbar,
                                     sal_uInt16 nPos,
                                     sal_uInt16 nItemId,
                                     const AddonStatusbarItem& rAddonItem )
{
    pStatusbar->InsertItem( nItemId,
                            rAddonItem.nWidth,
                            rAddonItem.nItemBits,
                            STATUSBAR_OFFSET,
                            nPos );
    pStatusbar->SetItemCommand( nItemId, rAddonItem.aCommandURL );
    pStatusbar->SetQuickHelpText( nItemId, rAddonItem.aLabel );
    pStatusbar->SetAccessibleName( nItemId, rAddonItem.aLabel );

    // add-on specific data
    AddonStatusbarItemData *pUserData = new AddonStatusbarItemData;
    pUserData->aLabel = rAddonItem.aLabel;
    pUserData->nItemBits = rAddonItem.nItemBits;
    pStatusbar->SetItemData( nItemId, pUserData );
}

static bool lcl_MergeItems( StatusBar* pStatusbar,
                            sal_uInt16 nPos,
                            sal_uInt16 nModIndex,
                            sal_uInt16& rItemId,
                            const ::rtl::OUString& rModuleIdentifier,
                            const AddonStatusbarItemContainer& rAddonItems )
{
    OSL_ENSURE(rAddonItems.size() <= 0xffff, "Caution: Not all items get handled (!)");
    const sal_uInt16 nSize( static_cast< sal_uInt16 >(rAddonItems.size()) );
    for ( sal_Int32 i = 0; i < nSize; i++ )
    {
        const AddonStatusbarItem& rItem = rAddonItems[i];
        if ( !StatusbarMerger::IsCorrectContext( rItem.aContext, rModuleIdentifier ) )
            continue;

        sal_uInt16 nInsPos = nPos + nModIndex + i;
        if ( nInsPos > pStatusbar->GetItemCount() )
            nInsPos = STATUSBAR_APPEND;

        lcl_CreateStatusbarItem( pStatusbar, nInsPos, rItemId, rItem );
        ++rItemId;
    }

    return true;
}

static bool lcl_ReplaceItem( StatusBar* pStatusbar,
                             sal_uInt16 nPos,
                             sal_uInt16& rItemId,
                            const ::rtl::OUString& rModuleIdentifier,
                             const AddonStatusbarItemContainer& rAddonToolbarItems )
{
    pStatusbar->RemoveItem( pStatusbar->GetItemId( nPos ) );
    return lcl_MergeItems( pStatusbar, nPos, 0, rItemId, rModuleIdentifier, rAddonToolbarItems );
}

static bool lcl_RemoveItems( StatusBar* pStatusbar,
                             sal_uInt16 nPos,
                             const ::rtl::OUString& rMergeCommandParameter )
{
    sal_Int32 nCount = rMergeCommandParameter.toInt32();
    if ( nCount > 0 )
    {
        for ( sal_Int32 i = 0; i < nCount; i++ )
        {
            if ( nPos < pStatusbar->GetItemCount() )
                pStatusbar->RemoveItem( nPos );
        }
    }
    return true;
}

}

bool StatusbarMerger::IsCorrectContext(
    const OUString& rContext,
    const OUString& rModuleIdentifier )
{
    return (( rContext.getLength() == 0 ) || ( rContext.indexOf( rModuleIdentifier ) >= 0 ));
}

bool StatusbarMerger::ConvertSeqSeqToVector(
    const Sequence< Sequence< PropertyValue > > &rSequence,
    AddonStatusbarItemContainer& rContainer )
{
    for ( sal_Int32 i = 0; i < rSequence.getLength(); i++ )
    {
        AddonStatusbarItem aStatusBarItem;
        lcl_ConvertSequenceToValues( rSequence[i], aStatusBarItem );
        rContainer.push_back( aStatusBarItem );
    }

    return true;
}

sal_uInt16 StatusbarMerger::FindReferencePos(
    StatusBar* pStatusbar,
    const OUString& rReferencePoint )
{
    for ( sal_uInt16 nPos = 0; nPos < pStatusbar->GetItemCount(); nPos++ )
    {
        const ::rtl::OUString rCmd = pStatusbar->GetItemCommand( pStatusbar->GetItemId( nPos ) );
        if ( rReferencePoint == rCmd )
            return nPos;
    }

    return STATUSBAR_ITEM_NOTFOUND;
}

bool StatusbarMerger::ProcessMergeOperation(
    StatusBar* pStatusbar,
    sal_uInt16 nPos,
    sal_uInt16& rItemId,
    const ::rtl::OUString& rModuleIdentifier,
    const ::rtl::OUString& rMergeCommand,
    const ::rtl::OUString& rMergeCommandParameter,
    const AddonStatusbarItemContainer& rItems )
{
    if ( rMergeCommand.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGECOMMAND_ADDAFTER ) ) )
        return lcl_MergeItems( pStatusbar, nPos, 1, rItemId, rModuleIdentifier, rItems );
    else if ( rMergeCommand.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGECOMMAND_ADDBEFORE ) ) )
        return lcl_MergeItems( pStatusbar, nPos, 0, rItemId, rModuleIdentifier, rItems );
    else if ( rMergeCommand.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGECOMMAND_REPLACE ) ) )
        return lcl_ReplaceItem( pStatusbar, nPos, rItemId, rModuleIdentifier, rItems );
    else if ( rMergeCommand.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGECOMMAND_REMOVE ) ) )
        return lcl_RemoveItems( pStatusbar, nPos, rMergeCommandParameter );

    return false;
}

bool StatusbarMerger::ProcessMergeFallback(
    StatusBar* pStatusbar,
    sal_uInt16 /*nPos*/,
    sal_uInt16& rItemId,
    const ::rtl::OUString& rModuleIdentifier,
    const ::rtl::OUString& rMergeCommand,
    const ::rtl::OUString& rMergeFallback,
    const AddonStatusbarItemContainer& rItems )
{
    // fallback IGNORE or REPLACE/REMOVE item not found
    if (( rMergeFallback.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGEFALLBACK_IGNORE ))) ||
            ( rMergeCommand.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM(  MERGECOMMAND_REPLACE ))) ||
            ( rMergeCommand.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM(  MERGECOMMAND_REMOVE  ))) )
    {
        return true;
    }
    else if (( rMergeCommand.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGECOMMAND_ADDBEFORE ))) ||
             ( rMergeCommand.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGECOMMAND_ADDAFTER ))) )
    {
        if ( rMergeFallback.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGEFALLBACK_ADDFIRST )))
            return lcl_MergeItems( pStatusbar, 0, 0, rItemId, rModuleIdentifier, rItems );
        else if ( rMergeFallback.equalsAsciiL( RTL_CONSTASCII_STRINGPARAM( MERGEFALLBACK_ADDLAST )))
            return lcl_MergeItems( pStatusbar, STATUSBAR_APPEND, 0, rItemId, rModuleIdentifier, rItems );
    }

    return false;
}


}

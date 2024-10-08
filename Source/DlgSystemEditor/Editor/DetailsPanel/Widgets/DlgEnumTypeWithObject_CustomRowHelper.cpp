// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "DlgEnumTypeWithObject_CustomRowHelper.h"

#include "DetailWidgetRow.h"
#include "Internationalization/TextNamespaceUtil.h"
#include "DlgSystemEditor/Editor/DetailsPanel/Widgets/DlgEditableTextPropertyHandle.h"
#include "DlgSystem/DlgHelper.h"
#include "SourceCodeNavigation.h"
#include "DlgSystemEditor/Editor/DetailsPanel/DlgDetailsPanelUtils.h"
#include "Kismet2/BlueprintEditorUtils.h"


#define LOCTEXT_NAMESPACE "DialogueEnumTypeWithObject_CustomRowHelper"
#define DEFAULT_FONT(...) FCoreStyle::GetDefaultFontStyle(__VA_ARGS__)

bool FDlgEnumTypeWithObject_CustomRowHelper::CanBeVisible() const
{
	if (!HasParticipantTag() || !Dialogue.IsValid() || !GetObject())
	{
		return false;
	}

	if (EnumType == EDialogueEnumWithObjectType::Condition)
	{
		return CanBeVisibleForConditionType();
	}
	if (EnumType == EDialogueEnumWithObjectType::Event)
	{
		return CanBeVisibleForEventType();
	}

	return false;
}

UObject* FDlgEnumTypeWithObject_CustomRowHelper::GetObject() const
{
	if (!Dialogue.IsValid())
	{
		return nullptr;
	}
	const FGameplayTag ParticipantTag = GetParticipantTag();
	if (!UBSDlgFunctions::IsValidParticipantTag(ParticipantTag))
	{
		return nullptr;
	}

	UClass* Class = Dialogue->GetParticipantClass(ParticipantTag);
	if (!Class)
	{
		return nullptr;
	}

	return Class->GetDefaultObject();
}

FText FDlgEnumTypeWithObject_CustomRowHelper::GetBrowseObjectText() const
{
	return LOCTEXT("BrowseButtonToolTipText", "Browse Participant Asset in Content Browser");
}

FText FDlgEnumTypeWithObject_CustomRowHelper::GetJumpToObjectText() const
{
	if (IsObjectABlueprint())
	{
		return LOCTEXT("OpenObjectBlueprintTooltipKey", "Open Participant Blueprint Editor");
	}

	// Native Class
	return FText::Format(
		LOCTEXT("OpenObjectBlueprintTooltipKey", "Open Participant Source File in {0}"),
		FSourceCodeNavigation::GetSelectedSourceCodeIDE()
	);
}

FReply FDlgEnumTypeWithObject_CustomRowHelper::OnOpenClicked()
{
	// Set function/event name to open
	if (EnumType == EDialogueEnumWithObjectType::Condition)
	{
		SetFunctionNameToOpen(EDlgBlueprintOpenType::Function, FDlgHelper::GetFunctionNameForConditionType(GetConditionType()));
	}
	if (EnumType == EDialogueEnumWithObjectType::Event)
	{
		SetFunctionNameToOpen(EDlgBlueprintOpenType::Function, FDlgHelper::GetFunctionNameForEventType(GetEventType()));
	}

	return Super::OnOpenClicked();
}

uint8 FDlgEnumTypeWithObject_CustomRowHelper::GetEnumValue() const
{
	if (!PropertyRow)
	{
		return 0;
	}

	uint8 Value = 0;
	TSharedPtr<IPropertyHandle> PropertyHandle = PropertyRow->GetPropertyHandle();
	if (!PropertyHandle.IsValid())
	{
		return 0;
	}
	PropertyHandle->GetValue(Value);
	return Value;
}

FGameplayTag FDlgEnumTypeWithObject_CustomRowHelper::GetParticipantTag() const
{
	if (!ParticipantTagPropertyHandle.IsValid())
	{
		return FGameplayTag::EmptyTag;
	}

	return FDlgDetailsPanelUtils::GetParticipantTagFromPropertyHandle(ParticipantTagPropertyHandle.ToSharedRef());
}

bool FDlgEnumTypeWithObject_CustomRowHelper::HasParticipantTag() const
{
	return UBSDlgFunctions::IsValidParticipantTag(GetParticipantTag());
}

#undef LOCTEXT_NAMESPACE
#undef DEFAULT_FONT

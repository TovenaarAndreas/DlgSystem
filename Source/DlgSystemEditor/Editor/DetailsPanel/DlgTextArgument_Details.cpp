// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "DlgTextArgument_Details.h"

#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "IDetailChildrenBuilder.h"
#include "UObject/TextProperty.h"

#include "DlgSystem/NYReflectionHelper.h"
#include "DlgDetailsPanelUtils.h"
#include "DlgSystemEditor/Editor/Nodes/DialogueGraphNode.h"
#include "DlgSystemEditor/Editor/DetailsPanel/Widgets/SDlgTextPropertyPickList.h"
#include "DlgSystemEditor/Editor/DetailsPanel/Widgets/DlgTextPropertyPickList_CustomRowHelper.h"
#include "DlgSystem/DlgHelper.h"
#include "DlgSystemEditor/Editor/DetailsPanel/Widgets/DlgObject_CustomRowHelper.h"

#define LOCTEXT_NAMESPACE "DialogueTextArgument_Details"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDialogueEventCustomization
void FDlgTextArgument_Details::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	Dialogue = FDlgDetailsPanelUtils::GetDialogueFromPropertyHandle(StructPropertyHandle.ToSharedRef());
	PropertyUtils = StructCustomizationUtils.GetPropertyUtilities();

	// Cache the Property Handle for the ArgumentType
	ParticipantTagPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgTextArgument, ParticipantTag));
	ArgumentTypePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgTextArgument, Type));
	check(ParticipantTagPropertyHandle.IsValid());
	check(ArgumentTypePropertyHandle.IsValid());

	// Register handler for event type change
	ArgumentTypePropertyHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateSP(this, &Self::OnArgumentTypeChanged, true));

	const bool bShowOnlyInnerProperties = StructPropertyHandle->GetProperty()->HasMetaData(META_ShowOnlyInnerProperties);
	if (!bShowOnlyInnerProperties)
	{
		HeaderRow.NameContent()
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			];
	}
}

void FDlgTextArgument_Details::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const bool bHasDialogue = Dialogue != nullptr;

	// DisplayString
	StructBuilder.AddProperty(StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgTextArgument, DisplayString)).ToSharedRef());

	// ParticipantTag
	{
		FDetailWidgetRow* DetailWidgetRow = &StructBuilder.AddCustomRow(LOCTEXT("ParticipantTagSearchKey", "Participant Tag"));

		ParticipantTagPropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgTextArgument, ParticipantTag)).ToSharedRef());
	}

	// ParticipantName
	{
		FDetailWidgetRow* DetailWidgetRow = &StructBuilder.AddCustomRow(LOCTEXT("ParticipantNameSearchKey", "Participant Name"));

		ParticipantNamePropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgTextArgument, ParticipantName)).ToSharedRef());
	}

	// ArgumentType
	StructBuilder.AddProperty(ArgumentTypePropertyHandle.ToSharedRef());

	// VariableName
	{
		const TSharedPtr<IPropertyHandle> VariableNamePropertyHandle = StructPropertyHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FDlgTextArgument, VariableName)
		);
		FDetailWidgetRow* DetailWidgetRow = &StructBuilder.AddCustomRow(LOCTEXT("VariableNameSearchKey", "Variable Name"));

		VariableNamePropertyRow = MakeShared<FDlgTextPropertyPickList_CustomRowHelper>(DetailWidgetRow, VariableNamePropertyHandle);
		VariableNamePropertyRow->SetTextPropertyPickListWidget(
				SNew(SDlgTextPropertyPickList)
				.AvailableSuggestions(this, &Self::GetAllDialoguesVariableNames)
				.OnTextCommitted(this, &Self::HandleTextCommitted)
				.HasContextCheckbox(bHasDialogue)
				.IsContextCheckBoxChecked(false)
				.CurrentContextAvailableSuggestions(this, &Self::GetCurrentDialogueVariableNames)
		);
		VariableNamePropertyRow->SetVisibility(CREATE_VISIBILITY_CALLBACK(&Self::GetVariableNameVisibility));
		VariableNamePropertyRow->Update();
	}

	// CustomTextArgument
	{
		CustomTextArgumentPropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgTextArgument, CustomTextArgument)).ToSharedRef()
		);
		CustomTextArgumentPropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetCustomTextArgumentVisibility));

		// Add Custom buttons
		CustomTextArgumentPropertyRow_CustomDisplay = MakeShared<FDlgObject_CustomRowHelper>(CustomTextArgumentPropertyRow);
		CustomTextArgumentPropertyRow_CustomDisplay->Update();
		CustomTextArgumentPropertyRow_CustomDisplay->SetFunctionNameToOpen(
			EDlgBlueprintOpenType::Function,
			GET_FUNCTION_NAME_CHECKED(UDlgTextArgumentCustom, GetText)
		);
	}

	// Cache the initial event type
	OnArgumentTypeChanged(false);
}

void FDlgTextArgument_Details::OnArgumentTypeChanged(bool bForceRefresh)
{
	// Update to the new type
	uint8 Value = 0;
	if (ArgumentTypePropertyHandle->GetValue(Value) != FPropertyAccess::Success)
	{
		return;
	}
	ArgumentType = static_cast<EDlgTextArgumentType>(Value);

	// Refresh the view, without this some names/tooltips won't get refreshed
	if (bForceRefresh && PropertyUtils.IsValid())
	{
		PropertyUtils->ForceRefresh();
	}
}

TArray<FName> FDlgTextArgument_Details::GetDialogueVariableNames(bool bCurrentOnly) const
{
	TArray<FName> Suggestions;
	const FGameplayTag ParticipantTag = FDlgDetailsPanelUtils::GetParticipantTagFromPropertyHandle(ParticipantTagPropertyHandle.ToSharedRef());

	switch (ArgumentType)
	{
		case EDlgTextArgumentType::DialogueInt:
			if (bCurrentOnly && Dialogue)
			{
				const TSet<FName> SuggestionsSet = Dialogue->GetParticipantIntNames(ParticipantTag);
				Suggestions = SuggestionsSet.Array();
			}
			else
			{
				Suggestions.Append(UDlgManager::GetDialoguesParticipantIntNames(ParticipantTag));
			}
			break;

		case EDlgTextArgumentType::ClassInt:
			FNYReflectionHelper::GetVariableNames(
				Dialogue->GetParticipantClass(ParticipantTag),
				FIntProperty::StaticClass(),
				Suggestions,
				GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
			);
			break;

		case EDlgTextArgumentType::DialogueFloat:
			if (bCurrentOnly && Dialogue)
			{
				const TSet<FName> SuggestionsSet = Dialogue->GetParticipantFloatNames(ParticipantTag);
				Suggestions = SuggestionsSet.Array();
			}
			else
			{
				Suggestions.Append(UDlgManager::GetDialoguesParticipantFloatNames(ParticipantTag));
			}
			break;

		case EDlgTextArgumentType::ClassFloat:
			if (Dialogue)
			{
				FNYReflectionHelper::GetVariableNames(
					Dialogue->GetParticipantClass(ParticipantTag),
					FDoubleProperty::StaticClass(),
					Suggestions,
					GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
				);
			}
			break;

		case EDlgTextArgumentType::ClassText:
			if (Dialogue)
			{
				FNYReflectionHelper::GetVariableNames(
					Dialogue->GetParticipantClass(ParticipantTag),
					FTextProperty::StaticClass(),
					Suggestions,
					GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
				);
			}
			break;

		case EDlgTextArgumentType::DisplayName:
		default:
			break;
	}
	FDlgHelper::SortFNameDefault(Suggestions);
	return Suggestions;
}

#undef LOCTEXT_NAMESPACE

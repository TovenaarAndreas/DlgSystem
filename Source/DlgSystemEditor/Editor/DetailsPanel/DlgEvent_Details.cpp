// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "DlgEvent_Details.h"

#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "IDetailChildrenBuilder.h"

#include "DlgSystem/NYReflectionHelper.h"
#include "DlgDetailsPanelUtils.h"
#include "DlgSystemEditor/Editor/Nodes/DialogueGraphNode.h"
#include "DlgSystemEditor/Editor/DetailsPanel/Widgets/SDlgTextPropertyPickList.h"
#include "DlgSystemEditor/Editor/DetailsPanel/Widgets/DlgTextPropertyPickList_CustomRowHelper.h"
#include "DlgSystem/DlgHelper.h"
#include "DlgSystemEditor/Editor/DetailsPanel/Widgets/DlgEnumTypeWithObject_CustomRowHelper.h"
#include "DlgSystemEditor/Editor/DetailsPanel/Widgets/DlgObject_CustomRowHelper.h"

#define LOCTEXT_NAMESPACE "DialogueEvent_Details"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FDialogueCustomEventization
void FDlgEvent_Details::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle,
	FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;
	Dialogue = FDlgDetailsPanelUtils::GetDialogueFromPropertyHandle(StructPropertyHandle.ToSharedRef());
	PropertyUtils = StructCustomizationUtils.GetPropertyUtilities();

	// Cache the Property Handle for the EventType
	ParticipantTagPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, ParticipantTag));
	EventTypePropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, EventType));
	check(ParticipantTagPropertyHandle.IsValid());
	check(EventTypePropertyHandle.IsValid());

	// Register handler for event type change
	EventTypePropertyHandle->SetOnPropertyValueChanged(
		FSimpleDelegate::CreateSP(this, &Self::OnEventTypeChanged, true)
	);

	const bool bShowOnlyInnerProperties = StructPropertyHandle->GetProperty()->HasMetaData(META_ShowOnlyInnerProperties);
	if (!bShowOnlyInnerProperties)
	{
		HeaderRow.NameContent()
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			];
	}
}

void FDlgEvent_Details::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	const bool bHasDialogue = Dialogue != nullptr;

	// Common ParticipantTag
	{
		ParticipantTagPropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, ParticipantTag)).ToSharedRef());
	}

	// Common ParticipantName
	{
		ParticipantNamePropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, ParticipantName)).ToSharedRef());
	}

	// EventType
	{
		EventTypePropertyRow = &StructBuilder.AddProperty(EventTypePropertyHandle.ToSharedRef());

		// Add Custom buttons
		EventTypePropertyRow_CustomDisplay = MakeShared<FDlgEnumTypeWithObject_CustomRowHelper>(
			EventTypePropertyRow,
			Dialogue,
			ParticipantTagPropertyHandle
		);
		EventTypePropertyRow_CustomDisplay->SetEnumType(EDialogueEnumWithObjectType::Event);
		EventTypePropertyRow_CustomDisplay->Update();
	}

	// EventName
	{
		const TSharedPtr<IPropertyHandle> EventNamePropertyHandle = StructPropertyHandle->GetChildHandle(
			GET_MEMBER_NAME_CHECKED(FDlgEvent, EventName)
		);
		FDetailWidgetRow* DetailWidgetRow = &StructBuilder.AddCustomRow(LOCTEXT("EventNameSearchKey", "Event Name"));

		EventNamePropertyRow = MakeShared<FDlgTextPropertyPickList_CustomRowHelper>(DetailWidgetRow, EventNamePropertyHandle);
		EventNamePropertyRow->SetTextPropertyPickListWidget(
				SNew(SDlgTextPropertyPickList)
				.AvailableSuggestions(this, &Self::GetDialoguesParticipantEventNames)
				.OnTextCommitted(this, &Self::HandleTextCommitted)
				.HasContextCheckbox(bHasDialogue)
				.IsContextCheckBoxChecked(false)
				.CurrentContextAvailableSuggestions(this, &Self::GetCurrentDialogueEventNames)
		)
		.SetVisibility(CREATE_VISIBILITY_CALLBACK(&Self::GetEventNameVisibility))
		.Update();
	}

	// IntValue
	{
		IntValuePropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, IntValue)).ToSharedRef()
		);
		IntValuePropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetIntValueVisibility));
	}

	// FloatValue
	{
		FloatValuePropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, FloatValue)).ToSharedRef()
		);
		FloatValuePropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetFloatValueVisibility));
	}

	// NameValue
	{
		NameValuePropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, NameValue)).ToSharedRef()
		);
		NameValuePropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetNameValueVisibility));
	}

	// bDelta
	{
		BoolDeltaPropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, bDelta)).ToSharedRef()
		);
		BoolDeltaPropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetBoolDeltaVisibility));
	}

	// bValue
	{
		BoolValuePropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, bValue)).ToSharedRef()
		);
		BoolValuePropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetBoolValueVisibility));
	}

	// CustomEvent
	{
		CustomEventPropertyRow = &StructBuilder.AddProperty(
			StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FDlgEvent, CustomEvent)).ToSharedRef()
		);
		CustomEventPropertyRow->Visibility(CREATE_VISIBILITY_CALLBACK(&Self::GetCustomEventVisibility));

		// Add Custom buttons
		CustomEventPropertyRow_CustomDisplay = MakeShared<FDlgObject_CustomRowHelper>(CustomEventPropertyRow);
		CustomEventPropertyRow_CustomDisplay->Update();
		CustomEventPropertyRow_CustomDisplay->SetFunctionNameToOpen(
			EDlgBlueprintOpenType::Event,
			GET_FUNCTION_NAME_CHECKED(UDlgEventCustom, EnterEvent)
		);
	}

	// Cache the initial event type
	OnEventTypeChanged(false);
}

void FDlgEvent_Details::OnEventTypeChanged(bool bForceRefresh)
{
	// Update to the new type
	uint8 Value = 0;
	if (EventTypePropertyHandle->GetValue(Value) != FPropertyAccess::Success)
	{
		return;
	}
	EventType = static_cast<EDlgEventType>(Value);

	// Update the display name/tooltips
	FText EventNameDisplayName = LOCTEXT("EventNameDisplayName", "Variable Name");
	FText EventNameToolTip = LOCTEXT("EventNameToolTip", "Name of the relevant variable");
	if (EventType == EDlgEventType::Event)
	{
		EventNameDisplayName = LOCTEXT("DlgEvent_EventNameDisplayName", "Event Name");
		EventNameToolTip = LOCTEXT("DlgEvent_EventNameToolTip", "Name of the relevant event");
	}
	else if (EventType == EDlgEventType::UnrealFunction)
	{
		EventNameDisplayName = LOCTEXT("DlgEvent_FunctionNameDisplayName", "Function Name");
		EventNameToolTip = LOCTEXT("DlgEvent_FunctionNameToolTip", "Name of the function the event will call on the participant");
	}

	EventNamePropertyRow->SetDisplayName(EventNameDisplayName)
		.SetToolTip(EventNameToolTip)
		.Update();

	// Refresh the view, without this some names/tooltips won't get refreshed
	if (bForceRefresh && PropertyUtils.IsValid())
	{
		PropertyUtils->ForceRefresh();
	}
}

TArray<FName> FDlgEvent_Details::GetDialoguesParticipantEventNames() const
{
	TArray<FName> Suggestions;
	const FGameplayTag ParticipantTag = FDlgDetailsPanelUtils::GetParticipantTagFromPropertyHandle(ParticipantTagPropertyHandle.ToSharedRef());

	switch (EventType)
	{
	case EDlgEventType::ModifyBool:
		Suggestions.Append(UDlgManager::GetDialoguesParticipantBoolNames(ParticipantTag));
		break;

	case EDlgEventType::ModifyFloat:
		Suggestions.Append(UDlgManager::GetDialoguesParticipantFloatNames(ParticipantTag));
		break;

	case EDlgEventType::ModifyInt:
		Suggestions.Append(UDlgManager::GetDialoguesParticipantIntNames(ParticipantTag));
		break;

	case EDlgEventType::ModifyName:
		Suggestions.Append(UDlgManager::GetDialoguesParticipantFNameNames(ParticipantTag));
		break;

	case EDlgEventType::ModifyClassIntVariable:
		if (Dialogue)
		{
			FNYReflectionHelper::GetVariableNames(
				Dialogue->GetParticipantClass(ParticipantTag),
				FIntProperty::StaticClass(),
				Suggestions,
				GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
			);
			FDlgHelper::SortFNameDefault(Suggestions);
		}
		break;

	case EDlgEventType::ModifyClassFloatVariable:
		if (Dialogue)
		{
			FNYReflectionHelper::GetVariableNames(
				Dialogue->GetParticipantClass(ParticipantTag),
				FDoubleProperty::StaticClass(),
				Suggestions,
				GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
			);
			FDlgHelper::SortFNameDefault(Suggestions);
		}
		break;

	case EDlgEventType::ModifyClassBoolVariable:
		if (Dialogue)
		{
			FNYReflectionHelper::GetVariableNames(
				Dialogue->GetParticipantClass(ParticipantTag),
				FBoolProperty::StaticClass(),
				Suggestions,
				GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
			);
			FDlgHelper::SortFNameDefault(Suggestions);
		}
		break;

	case EDlgEventType::ModifyClassNameVariable:
		if (Dialogue)
		{
			FNYReflectionHelper::GetVariableNames(
				Dialogue->GetParticipantClass(ParticipantTag),
				FNameProperty::StaticClass(),
				Suggestions,
				GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
			);
			FDlgHelper::SortFNameDefault(Suggestions);
		}
		break;

	case EDlgEventType::UnrealFunction:
		Suggestions.Append(GetParticipantFunctionNames(ParticipantTag).Array());
		break;

	case EDlgEventType::Event:
	default:
		Suggestions.Append(UDlgManager::GetDialoguesParticipantEventNames(ParticipantTag));
		break;
	}

	return Suggestions;
}

TArray<FName> FDlgEvent_Details::GetCurrentDialogueEventNames() const
{
	if (Dialogue == nullptr)
	{
		return {};
	}

	const FGameplayTag ParticipantTag = FDlgDetailsPanelUtils::GetParticipantTagFromPropertyHandle(ParticipantTagPropertyHandle.ToSharedRef());
	TSet<FName> Suggestions;

	switch (EventType)
	{
	case EDlgEventType::ModifyBool:
		Suggestions.Append(Dialogue->GetParticipantBoolNames(ParticipantTag));
		break;

	case EDlgEventType::ModifyName:
		Suggestions.Append(Dialogue->GetParticipantFNameNames(ParticipantTag));
		break;

	case EDlgEventType::ModifyFloat:
		Suggestions.Append(Dialogue->GetParticipantFloatNames(ParticipantTag));
		break;

	case EDlgEventType::ModifyInt:
		Suggestions.Append(Dialogue->GetParticipantIntNames(ParticipantTag));
		break;

	case EDlgEventType::ModifyClassIntVariable:
		FNYReflectionHelper::GetVariableNames(
			Dialogue->GetParticipantClass(ParticipantTag),
			FIntProperty::StaticClass(),
			Suggestions,
			GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
		);
		break;

	case EDlgEventType::ModifyClassFloatVariable:
		FNYReflectionHelper::GetVariableNames(
			Dialogue->GetParticipantClass(ParticipantTag),
			FDoubleProperty::StaticClass(),
			Suggestions,
			GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
		);
		break;

	case EDlgEventType::ModifyClassBoolVariable:
		FNYReflectionHelper::GetVariableNames(
			Dialogue->GetParticipantClass(ParticipantTag),
			FBoolProperty::StaticClass(),
			Suggestions,
			GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
		);
		break;

	case EDlgEventType::ModifyClassNameVariable:
		FNYReflectionHelper::GetVariableNames(
			Dialogue->GetParticipantClass(ParticipantTag),
			FNameProperty::StaticClass(),
			Suggestions,
			GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses
		);
		break;

	case EDlgEventType::UnrealFunction:
		Suggestions.Append(GetParticipantFunctionNames(ParticipantTag));
		break;

	case EDlgEventType::Event:
	default:
		Suggestions.Append(Dialogue->GetParticipantEventNames(ParticipantTag));
		break;
	}

	FDlgHelper::SortFNameDefault(Suggestions);
	return Suggestions.Array();
}

TSet<FName> FDlgEvent_Details::GetParticipantFunctionNames(const FGameplayTag& ParticipantTag) const
{
	if (Dialogue == nullptr)
	{
		return {};
	}
	UClass* ParticipantClass = Dialogue->GetParticipantClass(ParticipantTag);
	if (ParticipantClass == nullptr)
	{
		return {};
	}

	static const TArray<UClass*> BlacklistedClasses = GetDefault<UDlgSystemSettings>()->BlacklistedReflectionClasses;
	static const TArray<FName> BlacklistedFunctionNames = { TEXT("UserConstructionScript"), TEXT("ReceiveBeginPlay") };

	TSet<FName> PossibleFunctionNames;

	// Property link goes from the left to right where on the left there are the most inner child properties and at the right there are the top most parents.
	const UField* Field = ParticipantClass->Children;
	for (UClass* Class = ParticipantClass; Class != nullptr && !BlacklistedClasses.Contains(Field->GetOwnerClass()); Class = Class->GetSuperClass())
	{
		Field = Class->Children;
		while (Field != nullptr && !BlacklistedClasses.Contains(Field->GetOwnerClass()))
		{
			if (const UFunction* Function = Cast<UFunction>(Field))
			{
				// No arguments and name is not blacklisted
				if (Function->ParmsSize == 0 && !BlacklistedFunctionNames.Contains(Function->GetFName()))
				{
					PossibleFunctionNames.Add(Function->GetFName());
				}
			}
			Field = Field->Next;
		}
	}
	return PossibleFunctionNames;
}

#undef LOCTEXT_NAMESPACE

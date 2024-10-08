// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "DlgEventCustom.h"
#include "GameplayTagContainer.h"

#include "DlgEvent.generated.h"

class UDlgContext;
class UDlgDialogue;

UENUM(BlueprintType)
enum class EDlgEventType : uint8
{
	// Calls OnDialogueEvent on the Participant
	Event						UMETA(DisplayName = "Event", Hidden),

	// Calls ModifyIntValue on the Participant
	ModifyInt					UMETA(DisplayName = "Modify Dialogue Int Value", Hidden),

	// Calls ModifyFloatValue on the Participant
	ModifyFloat					UMETA(DisplayName = "Modify Dialogue Float Value", Hidden),

	// Calls ModifyBoolValue on the Participant
	ModifyBool					UMETA(DisplayName = "Modify Dialogue Bool Value", Hidden),

	// Calls ModifyNameValue on the Participant
	ModifyName					UMETA(DisplayName = "Modify Dialogue Name Value", Hidden),

	// Modifies the value from the Participant Int Variable
	ModifyClassIntVariable		UMETA(DisplayName = "Modify Class Int Variable", Hidden),

	// Modifies the value from the Participant Float Variable (Double Precision, double for c++)
	ModifyClassFloatVariable	UMETA(DisplayName = "Modify Class Float (Double Precision) Variable", Hidden),

	// Modifies the value from the Participant Bool Variable
	ModifyClassBoolVariable		UMETA(DisplayName = "Modify Class Bool Variable", Hidden),

	// Modifies the value from the Participant Name Variable
	ModifyClassNameVariable		UMETA(DisplayName = "Modify Class Name Variable", Hidden),

	// User Defined Event, calls EnterEvent on the custom event object.
	//
	// 1. Create a new Blueprint derived from DlgEventCustom (or DlgEventCustomHideCategories)
	// 2. Override EnterEvent
	Custom						UMETA(DisplayName = "Custom Event"),

	// Calls a regular Function (or Event) on the Participant (without any parameters)
	UnrealFunction				UMETA(DisplayName = "Unreal Function")
};


// Events are executed via calling IDlgDialogueParticipant methods on dialogue participants
// They must be handled in game side, can be used to modify game state based on dialogue
USTRUCT(BlueprintType)
struct DLGSYSTEM_API FDlgEvent
{
	GENERATED_USTRUCT_BODY()

public:
	//
	// ICppStructOps Interface
	//

	bool operator==(const FDlgEvent& Other) const
	{
		return ParticipantTag.MatchesTagExact(Other.ParticipantTag) &&
			EventName == Other.EventName &&
			IntValue == Other.IntValue &&
			FMath::IsNearlyEqual(FloatValue, Other.FloatValue, KINDA_SMALL_NUMBER) &&
			bDelta == Other.bDelta &&
			bValue == Other.bValue &&
			EventType == Other.EventType &&
			CustomEvent == Other.CustomEvent;
	}

	//
	// Own methods
	//

	// Executes the event
	// Participant is expected to implement IDlgDialogueParticipant interface
	void Call(UDlgContext& Context, const FString& ContextString, UObject* Participant) const;

	FString GetCustomEventName() const
	{
		return CustomEvent ? CustomEvent->GetName() : TEXT("INVALID");
	}

	static FString EventTypeToString(EDlgEventType Type);

	// Is this a Condition which has a Dialogue Value
	// NOTE: without EDlgConditionType::EventCall, for that call HasParticipantInterfaceValue
	static bool HasDialogueValue(EDlgEventType Type)
	{
		return Type == EDlgEventType::ModifyBool
			|| Type == EDlgEventType::ModifyFloat
			|| Type == EDlgEventType::ModifyInt
			|| Type == EDlgEventType::ModifyName;
	}

	// Same as HasDialogueValue but also Has the Event
	static bool HasParticipantInterfaceValue(EDlgEventType Type)
	{
		return Type == EDlgEventType::Event || HasDialogueValue(Type);
	}

	// Is this a Condition which has a Class Variable
	static bool HasClassVariable(EDlgEventType Type)
	{
		return Type == EDlgEventType::ModifyClassBoolVariable
			|| Type == EDlgEventType::ModifyClassFloatVariable
			|| Type == EDlgEventType::ModifyClassIntVariable
			|| Type == EDlgEventType::ModifyClassNameVariable;
	}

	FString GetEditorDisplayString(UDlgDialogue* OwnerDialogue) const;

protected:
	bool ValidateIsParticipantValid(const UDlgContext& Context, const FString& ContextString, const UObject* Participant) const;

	// Is the participant required?
	bool MustHaveParticipant() const { return EventType != EDlgEventType::Custom; }

	void CallUnrealFunction(UDlgContext& Context, const FString& ContextString, UObject* Participant) const;

public:
	// Name of the participant (speaker) the event is called on.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dialogue|Event", meta=(DeprecatedProperty, DeprecationMessage="Use ParticipantTag in stead!"))
	FName ParticipantName;

	// Tag of the participant (speaker) the event is called on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Event")
	FGameplayTag ParticipantTag;

	// Type of the event, can be a simple event or a call to modify a bool/int/float variable
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Event")
	EDlgEventType EventType = EDlgEventType::Event;

	// Name of the relevant variable or event
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Event")
	FName EventName;

	// The value the participant gets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Event")
	int32 IntValue = 0;

	// The value the participant gets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Event")
	double FloatValue = 0.f;

	// The value the participant gets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Event")
	FName NameValue;

	// Weather to add the value to the existing one, or simply override it
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Event")
	bool bDelta = false;

	// The value the participant gets
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue|Event")
	bool bValue = false;

	// User Defined Event, calls EnterEvent on the custom event object.
	//
	// 1. Create a new Blueprint derived from DlgEventCustom (or DlgEventCustomHideCategories)
	// 2. Override EnterEvent
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category = "Dialogue|Event")
	UDlgEventCustom* CustomEvent = nullptr;
};

template<>
struct TStructOpsTypeTraits<FDlgEvent> : public TStructOpsTypeTraitsBase2<FDlgEvent>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};

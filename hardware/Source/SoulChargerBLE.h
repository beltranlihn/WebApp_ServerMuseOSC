#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoulChargerBLE.generated.h"

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SOULCHARGER_API USoulChargerBLE : public UActorComponent
{
	GENERATED_BODY()

public:	
	USoulChargerBLE();

	// Initializes BLE GATT scanning for 0x180D (HR service)
	UFUNCTION(BlueprintCallable, Category="BioSensor|HR")
	bool ConnectHeartRateSensor();

	// Returns cached current Beats Per Minute (BPM)
	UFUNCTION(BlueprintCallable, Category="BioSensor|HR")
	float GetCurrentBPM();

private:
	bool bIsConnected;
	float CurrentBPM;
};

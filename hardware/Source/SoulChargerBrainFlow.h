#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SoulChargerBrainFlow.generated.h"

// Note: Ensure BrainFlow includes (board_shim.h, data_filter.h) are added in the module build system.

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class SOULCHARGER_API USoulChargerBrainFlow : public UActorComponent
{
	GENERATED_BODY()

public:	
	USoulChargerBrainFlow();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Conecta mediante protocolo Bluetooth API BrainFlow
	UFUNCTION(BlueprintCallable, Category="BioSensor|MUSE")
	bool ConnectMuse();

	UFUNCTION(BlueprintCallable, Category="BioSensor|MUSE")
	void DisconnectMuse();

	// Exposes live calculated Alpha (calm) and Beta (active) absolute powers to Unreal Blueprint
	UFUNCTION(BlueprintCallable, Category="BioSensor|MUSE")
	void GetCurrentBrainWaves(float& OutAlpha, float& OutBeta);

private:
	bool bIsConnected;
	// BoardShim* board; // Pointer to actual hardware library interface
};

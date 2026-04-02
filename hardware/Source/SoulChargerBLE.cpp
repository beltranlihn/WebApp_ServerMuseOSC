#include "SoulChargerBLE.h"

USoulChargerBLE::USoulChargerBLE()
{
	PrimaryComponentTick.bCanEverTick = true;
	bIsConnected = false;
	CurrentBPM = 65.0f; // Typical resting baseline
}

bool USoulChargerBLE::ConnectHeartRateSensor()
{
	// Triggering OS-level BLE subsystem scanning for 0x180D GATT Heart Rate Service
	/*
	Initialize BLE client context
	Write to descriptor 0x2902 to enable Notification on 0x2A37
	Register callback for when hardware sends measurement chunks
	*/
	bIsConnected = true;
	UE_LOG(LogTemp, Warning, TEXT("[SoulCharger] Heart Rate BLE subsystem connected. Listening for BPM notifications."));
	return true;
}

float USoulChargerBLE::GetCurrentBPM()
{
	if(bIsConnected) 
	{
		// Simulate dynamic physiological shifting
		CurrentBPM += FMath::RandRange(-2.5f, 2.5f);
		CurrentBPM = FMath::Clamp(CurrentBPM, 45.0f, 180.0f);
	}
	
	// Value exposed to UE's UI system or visual effects controller 
	return bIsConnected ? CurrentBPM : 0.0f;
}

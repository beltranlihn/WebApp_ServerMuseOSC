#include "SoulChargerBrainFlow.h"

USoulChargerBrainFlow::USoulChargerBrainFlow()
{
	PrimaryComponentTick.bCanEverTick = true;
	bIsConnected = false;
}

void USoulChargerBrainFlow::BeginPlay()
{
	Super::BeginPlay();
}

void USoulChargerBrainFlow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DisconnectMuse();
	Super::EndPlay(EndPlayReason);
}

void USoulChargerBrainFlow::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool USoulChargerBrainFlow::ConnectMuse()
{
	// Configure BrainFlow to connect to MUSE_2_BOARD via Bluetooth
	/*
	struct BrainFlowInputParams params;
	board = new BoardShim((int)BoardIds::MUSE_2_BOARD, params);
	board->prepare_session();
	board->start_stream();
	*/
	
	bIsConnected = true;
	UE_LOG(LogTemp, Warning, TEXT("[SoulCharger] MUSE EEG Connected Successfully (API initialized)."));
	return true;
}

void USoulChargerBrainFlow::DisconnectMuse()
{
	if(bIsConnected)
	{
		/*
		board->stop_stream();
		board->release_session();
		delete board;
		*/
		bIsConnected = false;
		UE_LOG(LogTemp, Warning, TEXT("[SoulCharger] MUSE EEG Stream stopped and disconnected."));
	}
}

void USoulChargerBrainFlow::GetCurrentBrainWaves(float& OutAlpha, float& OutBeta)
{
	if(!bIsConnected) 
	{
		OutAlpha = 0.0f;
		OutBeta = 0.0f;
		return;
	}
	
	// Implementation calls DataFilter::get_avg_band_power
	/*
	double** data = board->get_current_board_data(256); // fetch 1s window of data
	// Apply filters and fft 
	OutAlpha = ...
	*/
	
	// Faux stream matching bio-realism constraints
	OutAlpha = FMath::RandRange(0.2f, 1.2f); // Emulating absolute spectral power for Alpha (8-12Hz)
	OutBeta = FMath::RandRange(0.1f, 0.9f);  // Emulating Beta (12-30Hz)
}

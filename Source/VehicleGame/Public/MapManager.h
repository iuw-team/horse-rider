// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MapManager.generated.h"

typedef struct {
	bool is_occupied;
	bool neighbors[4];
	FName level_name;
	FQuat rotation;
} MapCell;

UCLASS()
class VEHICLEGAME_API AMapManager : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMapManager();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Map")
		void LoadChunksAt(FVector position, int radius_num);

	UFUNCTION(BlueprintCallable, Category = "Map")
		bool GenerateMap(FVector2D mapSize, int random_paths_num, double fill_probability, FVector2D start, FVector2D finish);

	UPROPERTY(EditAnywhere, Category = "Chunk size")
		double chunkSize;

	UPROPERTY(EditAnywhere, Category = "Map chunks")
		TArray<FName> NoRoadChunks;
	UPROPERTY(EditAnywhere, Category = "Map chunks")
		TArray<FName> DeadEndChunks;
	UPROPERTY(EditAnywhere, Category = "Map chunks")
		TArray<FName> Turn90Chunks;
	UPROPERTY(EditAnywhere, Category = "Map chunks")
		TArray<FName> StraightLineChunks;
	UPROPERTY(EditAnywhere, Category = "Map chunks")
		TArray<FName> TChunks;
	UPROPERTY(EditAnywhere, Category = "Map chunks")
		TArray<FName> CrossRoadChunks;

private: 
	TArray<MapCell> Map;
	TArray<std::tuple<ULevelStreaming*, FString>> LoadedChunks;
	FVector2D generatedMapSize;
};

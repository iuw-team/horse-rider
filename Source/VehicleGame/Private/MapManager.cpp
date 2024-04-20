// Fill out your copyright notice in the Description page of Project Settings.
#include "MapManager.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"

bool InBounds(int x, int y, int width, int height) {
	return x >= 0 && x < width && y >= 0 && y < height;
}

int IndexArray2D(int x, int y, int width) {
	return x + y * width;
}

FName GetRandomElem(TArray<FName> &array) {
	return array[FMath::RandRange(0, array.Num() - 1)];
}

// Draws line from start to finish taking only orthogonal steeps
// https://www.redblobgames.com/grids/line-drawing/
void DrawLine(TArray<bool>& array, int width, FVector2D p0, FVector2D p1) {
	double dx = p1.X - p0.X;
	double dy = p1.Y - p0.Y;
	double nx = abs(dx), ny = abs(dy);
	double sign_x = dx > 0.0 ? 1.0 : -1.0, sign_y = dy > 0 ? 1.0 : -1.0;

	FVector2D p = FVector2D(p0.X, p0.Y);
	for (double ix = 0, iy = 0; ix < nx || iy < ny;) {
		if ((1 + 2 * ix) * ny < (1 + 2 * iy) * nx) {
			// next step is horizontal;
			p.X += sign_x;
			ix++;
		}
		else {
			p.Y += sign_y;
			iy++;
		}
		array[IndexArray2D(p.X, p.Y, width)] = true;
	}
	array[IndexArray2D(p0.X, p0.Y, width)] = true;
	array[IndexArray2D(p1.X, p1.Y, width)] = true;
}

void DrawRandomLine(TArray<bool>& array, int width, FVector2D cur_point, FVector2D end_point) {
	while (cur_point != end_point) {
		int x_start = cur_point.X < end_point.X ? cur_point.X : end_point.X;
		int x_end = cur_point.X > end_point.X ? cur_point.X : end_point.X;
		int y_start = cur_point.Y < end_point.Y ? cur_point.Y : end_point.Y;
		int y_end = cur_point.Y > end_point.Y ? cur_point.Y : end_point.Y;

		double random_x = FMath::RandRange(x_start, x_end);
		double random_y = FMath::RandRange(y_start, y_end);

		FVector2D next_point = FVector2D(random_x, random_y);
		DrawLine(array, width, cur_point, next_point);
		cur_point = next_point;
	}
}


// Sets default values
AMapManager::AMapManager()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void AMapManager::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMapManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AMapManager::LoadChunksAt(FVector position, int radius_num)
{
	int x = ceil(position.X / chunkSize);
	int y = ceil(position.Y / chunkSize);
	
	// Filtering and unloading chunks which are out of bounds
	TArray<std::tuple<ULevelStreaming*, FString>> stillLoadedChunks;
	for (const auto &chunk : LoadedChunks) {
		auto location = get<0>(chunk)->LevelTransform.GetLocation();
		if (location.X < (x - radius_num) * chunkSize
			|| location.X > (x + radius_num) * chunkSize
			|| location.Y < (y - radius_num) * chunkSize
			|| location.Y > (y + radius_num) * chunkSize) {
			FLatentActionInfo LatentInfo;
			get<0>(chunk)->SetShouldBeLoaded(false);
			get<0>(chunk)->SetShouldBeVisible(false);
			UGameplayStatics::UnloadStreamLevel(this, FName(get<1>(chunk)), LatentInfo, false);
		}
		else {
			stillLoadedChunks.Add(chunk);
		}
	}

	// Loading new chunks 
	for (int ix = x - radius_num; ix < x + radius_num; ix++) {
		for (int iy = y - radius_num; iy < y + radius_num; iy++) {
			if (!InBounds(ix, iy, generatedMapSize.X, generatedMapSize.Y)) {
				continue;
			}

			bool already_loaded = false;
			for (const auto &chunk : LoadedChunks) {
				auto location = get<0>(chunk)->LevelTransform.GetLocation();
				if ((location.X == (ix * chunkSize)) && (location.Y == (iy * chunkSize))) {
					already_loaded = true;
					break;
				}
			}

			if (!already_loaded) {
				FString instance_name = "LevelX" + FString::FromInt(ix) + "Y" + FString::FromInt(iy);
				int index = IndexArray2D(ix, iy, generatedMapSize.X);
				if (index > Map.Num()) {
					if (GEngine)
						GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, TEXT("Something fucking terrible happened"));
					continue;
				}
				MapCell chunk = Map[index];

				auto stream_level = UGameplayStatics::GetStreamingLevel(this, FName(chunk.level_name));
				if (stream_level == nullptr) {
					if (GEngine)
						GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, TEXT("This fucker: " + chunk.level_name.ToString() + " did not exist..."));
					continue;
				}
				auto stream_data = stream_level->CreateInstance(instance_name);
				
				if (stream_data == nullptr) {
					stream_data = UGameplayStatics::GetStreamingLevel(this, FName(instance_name));
				}

				stream_data->LevelTransform.SetLocation(FVector(ix * chunkSize, iy * chunkSize, 0.0));
				stream_data->LevelTransform.SetRotation(chunk.rotation);

				stream_data->SetShouldBeLoaded(true);
				stream_data->SetShouldBeVisible(true);
				LoadedChunks.Add(std::make_tuple(stream_data, instance_name));
			}
		}
	}
}

bool AMapManager::GenerateMap(FVector2D mapSize, int random_paths_num, double fill_probability, FVector2D start, FVector2D finish)
{
	bool levels_good = true;
	for (const auto &array : {NoRoadChunks, DeadEndChunks, Turn90Chunks, StraightLineChunks, TChunks, CrossRoadChunks}) {
		for (const auto &level_name : array) {
			if (UGameplayStatics::GetStreamingLevel(this, level_name) == nullptr) {
				if (GEngine)
					GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red,
						TEXT("Level " + level_name.ToString() + " does not exists."));
				levels_good = false;
			};
		}
	}

	if (!levels_good) {
		return false;
	}

	if ((int)mapSize.X <= 0 || (int)mapSize.Y <= 0) {
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, "Invalid map size.");
		return false;
	}

	if (!InBounds(start.X, start.Y, mapSize.X, mapSize.Y)) {
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, "Start position is out of bounds.");
		return false;
	}

	if (!InBounds(finish.X, finish.Y, mapSize.X, mapSize.Y)) {
		if (GEngine)
			GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Yellow, "Finish position is out of bounds.");
		return false;
	}
	generatedMapSize = mapSize;

	FVector2D occupancyGridSize = mapSize + FVector2d(2, 2);
	TArray<bool> occupancyGrid;
	Map.Empty();

	// Initializing occupancy grid array with false values.
	// I love unreal :-)
	for (int i = 0; i < occupancyGridSize.X * occupancyGridSize.Y; i++) {
		occupancyGrid.Add(false);
	}

	FVector2D cur_point = start + FVector2D(1, 1);
	FVector2D end_point = finish + FVector2D(1, 1);
	for (int i = 0; i < random_paths_num; i++) {
		int random_x = FMath::RandRange(0, (int)mapSize.X - 1);
		int random_y = FMath::RandRange(0, (int)mapSize.Y - 1);
		FVector2D next_point = FVector2d(random_x, random_y);
		DrawRandomLine(occupancyGrid, occupancyGridSize.X, cur_point, next_point);
		cur_point = next_point;
	}
	DrawRandomLine(occupancyGrid, occupancyGridSize.X, cur_point, end_point);

	for (int i = 0; i < occupancyGrid.Num(); i++) {
		occupancyGrid[i] |= FMath::RandRange(0.0, 1.0) < fill_probability;
	}

	for (int y = 1; y <= mapSize.Y; y++) {
		for (int x = 1; x <= mapSize.X; x++) {
			bool middle = occupancyGrid[IndexArray2D(x, y, occupancyGridSize.X)];
			if (!middle) {
				Map.Add({ false, {false, false, false, false}, "EmptyLevel", FQuat::MakeFromEuler(FVector(0.0, 0.0, 180.0)) });
				continue;
			}
			bool top = occupancyGrid[IndexArray2D(x, y - 1, occupancyGridSize.X)];
			bool bottom = occupancyGrid[IndexArray2D(x, y + 1, occupancyGridSize.X)];
			bool left = occupancyGrid[IndexArray2D(x - 1, y, occupancyGridSize.X)];
			bool right = occupancyGrid[IndexArray2D(x + 1, y, occupancyGridSize.X)];
			MapCell cell = { true, {top, right, bottom, left}, "", FQuat() };
			int value = (int)top << 3 | (int)right << 2 | (int)bottom << 1 | (int)left;

			switch (value) {
			case 0b0000:
				cell.level_name = GetRandomElem(NoRoadChunks);
				break;
			case 0b0001: // Left
				cell.level_name = GetRandomElem(DeadEndChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 270.0));
				break;
			case 0b0010: // Bottom
				cell.level_name = GetRandomElem(DeadEndChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 180.0));
				break;
			case 0b0011: // Bottom Left
				cell.level_name = GetRandomElem(Turn90Chunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, -90.0));
				break;
			case 0b0100: // Right
				cell.level_name = GetRandomElem(DeadEndChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 90.0));
				break;
			case 0b0101: // Right Left =
				cell.level_name = GetRandomElem(StraightLineChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 90.0));
				break;
			case 0b0110: // Right Bottom
				cell.level_name = GetRandomElem(Turn90Chunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 180.0));
				break;
			case 0b0111: // Right Bottom Left
				cell.level_name = GetRandomElem(TChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 180.0));
				break;
			case 0b1000: // Top
				cell.level_name = GetRandomElem(DeadEndChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 0.0));
				break;
			case 0b1001: // Top Left
				cell.level_name = GetRandomElem(Turn90Chunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 0.0));
				break;
			case 0b1010: // Top Bottom =
				cell.level_name = GetRandomElem(StraightLineChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 0.0));
				break;
			case 0b1011: // Top Bottom Left
				cell.level_name = GetRandomElem(TChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, -90.0));
				break;
			case 0b1100: // Top Right
				cell.level_name = GetRandomElem(Turn90Chunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 90.0));
				break;
			case 0b1101: // Top Right Left
				cell.level_name = GetRandomElem(TChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 0.0));
				break;
			case 0b1110: // Top Right Bottom
				cell.level_name = GetRandomElem(TChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 90.0));
				break;
			case 0b1111: // Crossroad :-)
				cell.level_name = GetRandomElem(CrossRoadChunks);
				cell.rotation = FQuat::MakeFromEuler(FVector(0.0, 0.0, 0.0));
				break;
			}
			Map.Add(cell);
		}
	}

	return true;
}

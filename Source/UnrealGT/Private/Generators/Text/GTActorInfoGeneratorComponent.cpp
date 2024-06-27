// Fill out your copyright notice in the Description page of Project Settings.

#include "Generators/Text/GTActorInfoGeneratorComponent.h"

#include <CanvasItem.h>
#include <CanvasTypes.h>
#include <Components/StaticMeshComponent.h>
#include <Engine/Engine.h>
#include <Engine/StaticMesh.h>
#include <Engine/TextureRenderTarget2D.h>
#include <EngineUtils.h>
#include <Components/PrimitiveComponent.h>
#include "GTFileUtilities.h"
#include "Generators/Image/GTImageGeneratorBase.h"
#include "Generators/Image/GTSceneCaptureComponent2D.h"

UGTActorInfoGeneratorComponent::UGTActorInfoGeneratorComponent()
    : Super()
    , MinimalRequiredBoundingBoxSize(0, 0)
    , MaxDistanceToCamera(20000.f)
    , Header(TEXT(""))
    , FormatActorString(TEXT("{ActorName} {WorldLocation} {WorldRotation} {Velocity}"))
    , Separator(TEXT("\n"))
    , Footer(TEXT(""))
    , FormatVector2DString(TEXT("{X} {Y}"))
    , FormatVector3DString(TEXT("{X} {Y} {Z}"))
    , FormatRotatorString(TEXT("{Yaw} {Pitch} {Roll}"))
    , FormatVelocityString(TEXT("{X} {Y} {Z}"))
    , Format2DBoxString(TEXT("{Min} {Max} {Center} {Extent} {Width} {Height}"))
    , Format3DBoxString(TEXT("{Min} {Max} {Center} {Extent}"))
{
    SegmentationSceneCapture = CreateDefaultSubobject<UGTSceneCaptureComponent2D>(
        TEXT("InternalSegmentationSceneCapture"));
    SegmentationSceneCapture->SetupAttachment(this);

    // Set the tick function to run after physics
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
    PrimaryComponentTick.bCanEverTick = true;
}
void UGTActorInfoGeneratorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Generate data at each tick
    GenerateData(FDateTime::Now());
}
void UGTActorInfoGeneratorComponent::GenerateData(const FDateTime& TimeStamp)
{
    // Cleanup
    CachedBoundingBoxes.Empty();
    if (bAccurateBoundingBoxes && LinkedImageGenerator.GetComponent(GetOwner()))
    {
        UGTImageGeneratorBase* LinkedImageGeneratorComponent =
            Cast<UGTImageGeneratorBase>(LinkedImageGenerator.GetComponent(GetOwner()));

        SegmentationSceneCapture->SetResolution(
            LinkedImageGeneratorComponent->GetSceneCaptureComponent()->Resolution);
        SegmentationSceneCapture->CaptureImage(CachedSegmentation);
    }

    // GetTrackedaCtors
    TArray<AActor*> TrackedActors;

    for (TActorIterator<AActor> ActorItr(GetWorld()); ActorItr; ++ActorItr)
    {
        AActor* Actor = *ActorItr;
        for (const FGTObjectFilter& ObjectFilter : TrackActorsThatMatchFilter)
        {
            if (ObjectFilter.MatchesActor(Actor))
            {
                if (FVector::DistSquared2D(Actor->GetActorLocation(), GetComponentLocation()) <=
                    MaxDistanceToCamera * MaxDistanceToCamera)
                {
                    if (!bOnlyTrackOnScreenActors ||
                        (bOnlyTrackOnScreenActors &&
                         IsActorRenderedOnScreen(Actor)))
                    {
                        TrackedActors.Push(Actor);
                    }
                }
            }
        }
    }

    // Generate result string
    FString Result = TEXT("");

    Result.Append(Header);

    for (AActor* TrackedActor : TrackedActors)
    {
        UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(
            TrackedActor->GetComponentByClass(UStaticMeshComponent::StaticClass()));

        FString MeshName(TEXT("NOMESHONTHISACTOR"));
        if (MeshComp)
        {
            MeshComp->GetStaticMesh()->GetName(MeshName);
        }

        UGTImageGeneratorBase* LinkedImageGeneratorComponent =
            Cast<UGTImageGeneratorBase>(LinkedImageGenerator.GetComponent(GetOwner()));

        // Screen Location
        FVector2D ScreenLocation(-1.f, -1.f);
        FVector2D ScreenLocationNormalized;
        FBox2D ScreenBoundingBox;
        FBox2D ScreenBoundingBoxNormalized;
        if (LinkedImageGeneratorComponent)
        {
            LinkedImageGeneratorComponent->GetSceneCaptureComponent()->ProjectToPixelLocation(
                TrackedActor->GetActorLocation(), ScreenLocation);

            ScreenLocationNormalized =
                LinkedImageGeneratorComponent->GetSceneCaptureComponent()->NormalizePixelLocation(
                    ScreenLocation);

            GetActorScreenBoundingBox(
                TrackedActor, LinkedImageGeneratorComponent, ScreenBoundingBox);

            ScreenBoundingBoxNormalized.Min =
                LinkedImageGeneratorComponent->GetSceneCaptureComponent()->NormalizePixelLocation(
                    ScreenBoundingBox.Min);
            ScreenBoundingBoxNormalized.Max =
                LinkedImageGeneratorComponent->GetSceneCaptureComponent()->NormalizePixelLocation(
                    ScreenBoundingBox.Max);
        }

        // Get actor velocity using physics component
        FVector ActorVelocity(0.0f, 0.0f, 0.0f);

        // Iterate through components to find the one with physics simulation enabled
        TArray<UPrimitiveComponent*> PrimitiveComponents;
        TrackedActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
        for (UPrimitiveComponent* Component : PrimitiveComponents)
        {
            // if (Component->IsSimulatingPhysics())
            // {
            //     ActorVelocity = Component->GetPhysicsLinearVelocity();
            //     break; // Found the component, no need to check further
            // }
            ActorVelocity = Component->GetPhysicsLinearVelocity();
            UE_LOG(LogTemp, Warning, TEXT("Actor: %s, Component: %s, Velocity: %s"),
            *TrackedActor->GetName(),
            *Component->GetName(),
            *ActorVelocity.ToString());
        }

        // Convert Unreal units to meters (assuming Unreal units are centimeters)
        ActorVelocity /= 100.0f;

        TMap<FString, FStringFormatArg> GlobalProperties{
            {TEXT("WorldLocation"), Vector3DToFormattedString(TrackedActor->GetActorLocation())},
            {TEXT("WorldRotation"), RotatorToFormattedString(TrackedActor->GetActorRotation())},
            {TEXT("ScreenLocation"), Vector2DToFormattedString(ScreenLocation)},
            {TEXT("ScreenLocationNormalized"), Vector2DToFormattedString(ScreenLocationNormalized)},
            {TEXT("ScreenBoundingBox"), Box2DToFormattedString(ScreenBoundingBox)},
            {TEXT("ScreenBoundingBoxNormalized"),
             Box2DToFormattedString(ScreenBoundingBoxNormalized)},
            {TEXT("ActorName"), TrackedActor->GetName()},
            {TEXT("Velocity"), VelocityToFormattedString(ActorVelocity)},
            {TEXT("MeshName"), MeshName}};

        FString ActorResult = FString::Format(*FormatActorString, GlobalProperties);

        for (const TPair<FString, FString>& ReplacePair : ReplaceStrings)
        {
            ActorResult.ReplaceInline(
                *ReplacePair.Key, *ReplacePair.Value, ESearchCase::CaseSensitive);
        }

        Result.Append(ActorResult);
        Result.Append(Separator);
    }

    Result.RemoveFromEnd(Separator);
    Result.Append(Footer);

    CurrentResult = Result.ReplaceEscapedCharWithChar();

    const auto Data = FGTFileUtilities::StringToCharArray(CurrentResult);

    DataReadyDelegate.Broadcast(Data, TimeStamp);
}

void UGTActorInfoGeneratorComponent::DrawDebug(FViewport* Viewport, FCanvas* Canvas)
{
    float TextOffset = 0.f;
    Canvas->Clear(FLinearColor::White);

    if (LinkedImageGenerator.GetComponent(GetOwner()))
    {
        // Draw boundingboxes
        UGTImageGeneratorBase* LinkedImageGeneratorComponent =
            Cast<UGTImageGeneratorBase>(LinkedImageGenerator.GetComponent(GetOwner()));

        UTextureRenderTarget2D* LinkedImageTextureTarget =
            LinkedImageGeneratorComponent->GetSceneCaptureComponent()->TextureTarget;
        FTexture* LinkedImageTextureResource = LinkedImageTextureTarget->GetResource();
        FCanvasTileItem LinkedImageItem(
            FVector2D(0.f, 0.f),
            LinkedImageTextureResource,
            FVector2D(LinkedImageTextureTarget->SizeX, LinkedImageTextureTarget->SizeY),
            FVector2D::ZeroVector,
            FVector2D::ZeroVector + FVector2D(1.f, 1.f),
            FLinearColor::White);
        LinkedImageItem.Rotation = FRotator(0.f, 0.f, 0.f);
        LinkedImageItem.PivotPoint = FVector2D(0.5f, 0.5f);
        LinkedImageItem.BlendMode = FCanvas::BlendToSimpleElementBlend(EBlendMode::BLEND_Opaque);
        Canvas->DrawItem(LinkedImageItem);
        for (const auto& ActorBoundingBoxPair : CachedBoundingBoxes)
        {
            // we need to add 1 because the canvas box item draws around the specified coords?????
            FVector2D VectorOffset(1.f, 1.f);
            FCanvasBoxItem BoxItem(
                ActorBoundingBoxPair.Value.Min + VectorOffset,
                ActorBoundingBoxPair.Value.GetSize());
            BoxItem.SetColor(FColor::Red);
            BoxItem.LineThickness = 1.f;
            Canvas->DrawItem(BoxItem);
        }
        TextOffset += LinkedImageItem.Size.Y;

        if (bAccurateBoundingBoxes && SegmentationSceneCapture)
        {
            if (SegmentationSceneCapture && SegmentationSceneCapture->TextureTarget &&
                SegmentationSceneCapture->TextureTarget->IsValidLowLevel())
            {
                // Draw segmetnation
                UTextureRenderTarget2D* SegmentationTextureTarget =
                    SegmentationSceneCapture->TextureTarget;
                FTexture* SegmentationResource = SegmentationTextureTarget->GetResource();
                FCanvasTileItem SegmentationItem(
                    FVector2D(LinkedImageItem.Size.X, 0.f),
                    SegmentationResource,
                    FVector2D(SegmentationTextureTarget->SizeX, SegmentationTextureTarget->SizeY),
                    FVector2D::ZeroVector,
                    FVector2D::ZeroVector + FVector2D(1.f, 1.f),
                    FLinearColor::White);
                SegmentationItem.Rotation = FRotator(0.f, 0.f, 0.f);
                SegmentationItem.PivotPoint = FVector2D(0.5f, 0.5f);
                SegmentationItem.BlendMode =
                    FCanvas::BlendToSimpleElementBlend(EBlendMode::BLEND_Opaque);
                Canvas->DrawItem(SegmentationItem);

                // Draw colormap
                UTexture2D* ColorMap = SegmentationSceneCapture->ColorMap;
                FTexture* ColorMapResource = ColorMap->GetResource();
                FCanvasTileItem ColorMapItem(
                    FVector2D(0.f, SegmentationItem.Size.Y),
                    ColorMapResource,
                    FVector2D(ColorMapResource->GetSizeX(), ColorMapResource->GetSizeY() * 8.f),
                    FVector2D::ZeroVector,
                    FVector2D::ZeroVector + FVector2D(1.f, 1.f),
                    FLinearColor::White);
                ColorMapItem.Rotation = FRotator(0.f, 0.f, 0.f);
                ColorMapItem.PivotPoint = FVector2D(0.5f, 0.5f);
                ColorMapItem.BlendMode =
                    FCanvas::BlendToSimpleElementBlend(EBlendMode::BLEND_Opaque);
                Canvas->DrawItem(ColorMapItem);
                TextOffset += ColorMapItem.Size.Y;
            }
        }
    }

    FCanvasTextItem TextItem(
        FVector2D(0.f, TextOffset),
        FText::FromString(CurrentResult.Mid(0, 5000)),
        GEngine->GetMediumFont(),
        FLinearColor::Red);
    Canvas->DrawItem(TextItem);
}

#if WITH_EDITOR

bool UGTActorInfoGeneratorComponent::CanEditChange(const FProperty* InProperty) const
{
    if (InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGTActorInfoGeneratorComponent, bOnlyTrackOnScreenActors) || InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UGTActorInfoGeneratorComponent, bAccurateBoundingBoxes))
    {
        if (!LinkedImageGenerator.IsSet())
        {
            return false;
        }
    }
    
    return Super::CanEditChange(InProperty);
}

#endif

void UGTActorInfoGeneratorComponent::BeginPlay()
{
    Super::BeginPlay();

    if (bAccurateBoundingBoxes && LinkedImageGenerator.GetComponent(GetOwner()))
    {
        UGTImageGeneratorBase* LinkedImageGeneratorComponent =
            Cast<UGTImageGeneratorBase>(LinkedImageGenerator.GetComponent(GetOwner()));

        SegmentationSceneCapture->SetResolution(
            LinkedImageGeneratorComponent->GetSceneCaptureComponent()->Resolution);
        SegmentationSceneCapture->SetupSegmentationPostProccess(
            TrackActorsThatMatchFilter, bShouldApplyCloseMorph);
    }
}

bool UGTActorInfoGeneratorComponent::IsActorRenderedOnScreen(AActor* Actor)
{
    UGTImageGeneratorBase* LinkedImageGeneratorComponent =
        Cast<UGTImageGeneratorBase>(LinkedImageGenerator.GetComponent(GetOwner()));
    if (LinkedImageGeneratorComponent)
    {
        FBox2D ScreenBoundingBox;
        GetActorScreenBoundingBox(Actor, LinkedImageGeneratorComponent, ScreenBoundingBox);
        const auto ScreenBoundingBoxSize = ScreenBoundingBox.GetSize();

        if (!bRequireMinimumVisibleBoundingBox && !ScreenBoundingBoxSize.IsNearlyZero())
        {
            return true;
        }
        
        if (ScreenBoundingBoxSize.X < MinimalRequiredBoundingBoxSize.X ||
            ScreenBoundingBoxSize.Y < MinimalRequiredBoundingBoxSize.Y)
        {
            return false;
        }
    }

    return true;
}

bool UGTActorInfoGeneratorComponent::GetActorScreenBoundingBox(
    AActor* InActor,
    UGTImageGeneratorBase* ImageGeneratorComponent,
    FBox2D& OutBox)
{
    if (CachedBoundingBoxes.Contains(InActor))
    {
        OutBox = CachedBoundingBoxes[InActor];
        return true;
    }

    FVector ActorCenter;
    FVector ActorExtent;
    InActor->GetActorBounds(false, ActorCenter, ActorExtent);

    const FVector BoundsPointMapping[8] = {FVector(1, 1, 1),
                                           FVector(1, 1, -1),
                                           FVector(1, -1, 1),
                                           FVector(1, -1, -1),
                                           FVector(-1, 1, 1),
                                           FVector(-1, 1, -1),
                                           FVector(-1, -1, 1),
                                           FVector(-1, -1, -1)};
    FBox2D ScreenBoundingBox(EForceInit::ForceInitToZero);

    for (uint8 BoundsPointItr = 0; BoundsPointItr < 8; BoundsPointItr++)
    {
        FVector2D ProjectedWorldLocation;

        bool bValidPixelLocation =
            ImageGeneratorComponent->GetSceneCaptureComponent()->ProjectToPixelLocation(
                ActorCenter + (BoundsPointMapping[BoundsPointItr] * ActorExtent),
                ProjectedWorldLocation);

        if (bValidPixelLocation)
        {
            ScreenBoundingBox += FVector2D(ProjectedWorldLocation.X, ProjectedWorldLocation.Y);
        }
    }

    if (bAccurateBoundingBoxes && CachedSegmentation.IsValid())
    {
        FBox2D AccurateScreenBoundingBox(EForceInit::ForceInitToZero);

        TArray<FColor> SegmentColors =
            SegmentationSceneCapture->GetSegmentColorsUsedForActor(InActor);
        int TotalPixelOverlap = 0;
        for (const FColor& SegmentColor : SegmentColors)
        {
            for (int X = ScreenBoundingBox.Min.X; X <= ScreenBoundingBox.Max.X; X++)
            {
                for (int Y = ScreenBoundingBox.Min.Y; Y <= ScreenBoundingBox.Max.Y; Y++)
                {
                    FColor ColorInSegmentationMap = CachedSegmentation.GetPixel(X, Y);
                    if (SegmentColor == ColorInSegmentationMap)
                    {
                        AccurateScreenBoundingBox += FVector2D(X, Y);
                        TotalPixelOverlap++;
                    }
                }
            }
        }
        // Discard bbox if not enough pixels are detetected inside the original box
        // This will not work great if the detect object is a ring shaped or similiar
        // TODO make configurable
        if (TotalPixelOverlap <= ScreenBoundingBox.GetArea() / 8)
        {
            AccurateScreenBoundingBox = FBox2D(EForceInit::ForceInitToZero);
        }

        ScreenBoundingBox = AccurateScreenBoundingBox;
    }

    CachedBoundingBoxes.Add(InActor, ScreenBoundingBox);
    OutBox = ScreenBoundingBox;

    return true;
}

FString UGTActorInfoGeneratorComponent::VelocityToFormattedString(const FVector& InVelocity)
{
    return FString::Format(
        *FormatVelocityString,
        {{TEXT("X"), InVelocity.X},
         {TEXT("Y"), InVelocity.Y},
         {TEXT("Z"), InVelocity.Z}});
}

FString UGTActorInfoGeneratorComponent::Vector2DToFormattedString(const FVector2D& InVector)
{
    return FString::Format(
        *FormatVector2DString, {{TEXT("X"), InVector.X}, {TEXT("Y"), InVector.Y}});
}

FString UGTActorInfoGeneratorComponent::Vector3DToFormattedString(const FVector& InVector)
{
    return FString::Format(
        *FormatVector3DString,
        {{TEXT("X"), InVector.X}, {TEXT("Y"), InVector.Y}, {TEXT("Z"), InVector.Z}});
}

FString UGTActorInfoGeneratorComponent::RotatorToFormattedString(const FRotator& InRotator)
{
    return FString::Format(
        *FormatRotatorString,
        {{TEXT("Yaw"), InRotator.Yaw},
         {TEXT("Pitch"), InRotator.Pitch},
         {TEXT("Roll"), InRotator.Roll}});
}

FString UGTActorInfoGeneratorComponent::Box2DToFormattedString(const FBox2D& InBox)
{
    return FString::Format(
        *Format2DBoxString,
        {{TEXT("Min"), Vector2DToFormattedString(InBox.Min)},
         {TEXT("Max"), Vector2DToFormattedString(InBox.Max)},
         {TEXT("Width"), InBox.GetSize().X},
         {TEXT("Height"), InBox.GetSize().Y},
         {TEXT("Center"), Vector2DToFormattedString(InBox.GetCenter())},
         {TEXT("Extent"), Vector2DToFormattedString(InBox.GetExtent())}});
}

FString UGTActorInfoGeneratorComponent::Box3DToFormattedString(const FBox& InBox)
{
    return FString::Format(
        *Format3DBoxString,
        {{TEXT("Min"), Vector3DToFormattedString(InBox.Min)},
         {TEXT("Max"), Vector3DToFormattedString(InBox.Max)},
         {TEXT("Center"), Vector3DToFormattedString(InBox.GetCenter())},
         {TEXT("Extent"), Vector3DToFormattedString(InBox.GetExtent())}});
}

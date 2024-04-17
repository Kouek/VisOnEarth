// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MCSActor.h"
#include "MCSUIWidget.generated.h"

/**
 * 
 */
UCLASS()
class VIS4EARTH_API UMCSUIWidget : public UUserWidget {
    GENERATED_BODY()
public:

    UFUNCTION(BlueprintCallable)
    void SetMCSActor(AMCSActor* InActor);
    
protected:
    void NativeConstruct() override;
    
private:
    
    UPROPERTY(meta = (BindWidget))
    class UButton* LoadRAWVolumeButton;
    
    UPROPERTY(meta = (BindWidget))
    class UButton* LoadTFButton;
    
    UPROPERTY(meta = (BindWidget))
    class UComboBoxString* SmoothComboBoxString;
    
    UPROPERTY(meta = (BindWidget))
    class UComboBoxString* InterpComboBoxString;

    UPROPERTY(meta = (BindWidget))
    class UComboBoxString* LineTypeComboBoxString;

    
    UFUNCTION()
    void OnLoadRAWVolumeButtonClicked();
    
    UFUNCTION()
    void OnLoadTFButtonClicked();
    
    UFUNCTION()
    void OnSmoothComboBoxStringSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnInterpComboBoxStringSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void OnLineTypeComboBoxStringSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);


    TWeakObjectPtr<AMCSActor> MCSAtor;
    
};

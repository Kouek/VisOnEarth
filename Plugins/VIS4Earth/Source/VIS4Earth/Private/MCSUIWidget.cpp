// Fill out your copyright notice in the Description page of Project Settings.


#include "MCSUIWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"


void UMCSUIWidget::NativeConstruct() {
    Super::NativeConstruct();

    AddToViewport(0);

    LoadRAWVolumeButton->OnClicked.AddDynamic(this, &UMCSUIWidget::OnLoadRAWVolumeButtonClicked);
    LoadTFButton->OnClicked.AddDynamic(this, &UMCSUIWidget::OnLoadTFButtonClicked);
    SmoothComboBoxString->OnSelectionChanged.AddDynamic(
        this, &UMCSUIWidget::OnSmoothComboBoxStringSelectionChanged);
    InterpComboBoxString->OnSelectionChanged.AddDynamic(
        this, &UMCSUIWidget::OnInterpComboBoxStringSelectionChanged);
    LineTypeComboBoxString->OnSelectionChanged.AddDynamic(
        this, &UMCSUIWidget::OnLineTypeComboBoxStringSelectionChanged);
}
void UMCSUIWidget::SetMCSActor(AMCSActor *InActor) {
    MCSAtor = InActor;
}

void UMCSUIWidget::OnLoadTFButtonClicked() {

}

void UMCSUIWidget::OnLoadRAWVolumeButtonClicked() {
    if(MCSAtor.IsValid()){
        MCSAtor->VolumeComponent->LoadRAWVolume();
    }else {
        UE_LOG(LogTemp, Warning, TEXT("UMCSUIWidget: MCSActor is Invalid!"));
    }
}

void UMCSUIWidget::OnInterpComboBoxStringSelectionChanged(FString SelectedItem,
                                                          ESelectInfo::Type SelectionType) {

    uint32 Selected = InterpComboBoxString->GetSelectedIndex();
}


void UMCSUIWidget::OnSmoothComboBoxStringSelectionChanged(FString SelectedItem,
                                                          ESelectInfo::Type SelectionType) {
    uint32 Selected = SmoothComboBoxString->GetSelectedIndex();
}

void UMCSUIWidget::OnLineTypeComboBoxStringSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType) {

    uint32 Selected = LineTypeComboBoxString->GetSelectedIndex();
}

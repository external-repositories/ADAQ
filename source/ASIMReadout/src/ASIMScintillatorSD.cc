/////////////////////////////////////////////////////////////////////////////////
//
// name: ASIMScintillatorSD.cc
// date: 21 May 15
// auth: Zach Hartwig
// mail: hartwig@psfc.mit.edu
//
// desc: ASIMScintillatorSD class is intended as a generic Geant4
//       sensitive detector class that should be attached to
//       scintillator volumes to handle data readout into the ASIM
//       file format supported by the ADAQ framework. It ensures that
//       the essential information for a scintillator is made
//       available for readout: energy deposition, hit
//       position/momentum, scintillation photons created, and
//       particle type. The class should be used with the
//       ASIMScintillatorSDHit class.
//
/////////////////////////////////////////////////////////////////////////////////

#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"
#include "G4HCofThisEvent.hh"
#include "G4TouchableHistory.hh"
#include "G4ios.hh"
#include "G4THitsMap.hh"
#include "G4SDManager.hh"
#include "G4ParticleTypes.hh"
#include "G4ParticleDefinition.hh"
#include "G4EventManager.hh"
#include "G4Event.hh"
#include "Randomize.hh"
#include "G4SystemOfUnits.hh"

#include "ASIMScintillatorSD.hh"
#include "ASIMScintillatorSDHit.hh"


ASIMScintillatorSD::ASIMScintillatorSD(G4String name)
  : G4VSensitiveDetector(name),
    hitColour(new G4Colour(1.0, 0.0, 0.0, 1.0)), hitSize(4)
{ InitializeCollections(name); }


ASIMScintillatorSD::ASIMScintillatorSD(G4String name,
				       G4Colour *colour,
				       G4double size)
  : G4VSensitiveDetector(name),
    hitColour(colour), hitSize(size)
{ InitializeCollections(name); }


void ASIMScintillatorSD::InitializeCollections(G4String name)
{
  G4String theCollectionName = name + "Collection";
  
  // A public list that can be accessed from other classes
  // for convenience of obtaining the collection names (mine)
  collectionNameList.push_back(theCollectionName);
  
  // A mandatory protected list only for this class (Geant4)
  // that must contain name(s) of desired hit collection(s)
  collectionName.insert(collectionNameList.at(0));
}


ASIMScintillatorSD::~ASIMScintillatorSD()
{ collectionNameList.clear(); }


void ASIMScintillatorSD::Initialize(G4HCofThisEvent *HCE)
{
  static int HCID = -1;
  
  hitCollection = new ASIMScintillatorSDHitCollection(SensitiveDetectorName,
						      collectionName[0]); 
  
  if(HCID<0){
    HCE->AddHitsCollection(GetCollectionID(0),
			   hitCollection);
  }
}


G4bool ASIMScintillatorSD::ProcessHits(G4Step *currentStep, G4TouchableHistory *)
{
  G4Track *currentTrack = currentStep->GetTrack();
  
  // Ensure that optical photons are excluded from registering hits
  if(currentTrack->GetDefinition() != G4OpticalPhoton::OpticalPhotonDefinition()){
    
    ASIMScintillatorSDHit *newHit = new ASIMScintillatorSDHit(hitColour, hitSize);
    
    // Obtain the quantities from the step/track objects
    G4double energyDep = currentStep->GetTotalEnergyDeposit() * currentTrack->GetWeight();
    G4double kineticEnergy = currentTrack->GetKineticEnergy();
    G4ThreeVector position = currentTrack->GetPosition();
    G4ThreeVector momentumDir = currentTrack->GetMomentumDirection();
    G4ParticleDefinition *particleDef = currentTrack->GetDefinition();

    // Set the quantities to the SD hit class
    newHit->SetEnergyDep(energyDep);
    newHit->SetKineticEnergy(kineticEnergy);
    newHit->SetPosition(position);
    newHit->SetMomentumDir(momentumDir);
    newHit->SetIsOpticalPhoton(false);
    newHit->SetParticleDef(particleDef);

    // Insert the SD hit into the SD hit collection
    hitCollection->insert(newHit);
  }
  return true;
}


G4bool ASIMScintillatorSD::ManualTrigger(const G4Track *CurrentTrack)
{
  // Ensure that currently tracking particle is an optical photon
  if(CurrentTrack->GetDefinition() != G4OpticalPhoton::OpticalPhotonDefinition())
    return false;
  
  ASIMScintillatorSDHit *newHit = new ASIMScintillatorSDHit();
  newHit->SetIsOpticalPhoton(true);
  hitCollection->insert(newHit);
  
  return true; 
}


void ASIMScintillatorSD::EndOfEvent(G4HCofThisEvent *)
{;}

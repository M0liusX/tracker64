#include "pianoroll.hpp"
#include <algorithm>
#include <iostream>
#include <string>
#include <cmath>

ImVec2 trackScroll = { 0.f, 0.f };
ImVec2 trackOffset = { 100.f, -1590.0f };
ImVec2 trackScale = { .1f, 3.5f };
int trackID = 0;
bool wasDown = 0;
float scrollSpeed;
u32 division;
u64 lastEffectDelta;

void RenderNote(Command64* command, ImVec2& offset, ImDrawList* drawList) {
   bool isNote = (command->status & 0xf0) == AL_MIDI_NoteOn;
   // TODO: save a decoded duration
   u64 duration = division / 2;
   if (isNote) {
      std::vector<u8> encDuration;
      encDuration.insert(encDuration.begin(), command->bytes.begin() + 2, command->bytes.end());
      duration = Track64::DecodeDelta(encDuration);
   }
   u64 delta = (command->edit.currState == EditState::DRAGGING) ? command->edit.delta : command->delta;
   
   /* Remove stacked program changes */
   //if (!isNote) {
   //   if (delta <= lastEffectDelta) {
   //      lastEffectDelta += division;
   //      delta = lastEffectDelta;
   //   }
   //}
   float pitch = 0;
   if (isNote) {
      pitch = (command->edit.currState == EditState::DRAGGING) ? (0x100 - command->edit.note) * 10 - 5 : 
                                                                 (0x100 - command->bytes[0])  * 10 - 5;
   }

   static const float height = 10.0f;
   static const ImU32 yellow = IM_COL32(0xFF, 0xFF,    0, 0xFF);
   static const ImU32 red    = IM_COL32(0xFF,    0,    0, 0xFF);
   float X = offset.x + (delta + trackOffset.x) * trackScale.x;
   float Y = offset.y + (trackOffset.y + pitch) * trackScale.y;
   float X2 = offset.x + ((delta + duration) + trackOffset.x) * trackScale.x;
   if (!isNote) { pitch = 0x1000; }
   float Y2 = offset.y + (trackOffset.y + (pitch + height)) * trackScale.y;


   ImVec2 mousePos = ImGui::GetMousePos();
   bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
   bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
   bool hover = false;
   if ((mousePos.x >= X && mousePos.x <= X2) &&
       (mousePos.y >= Y && mousePos.y <= Y2)) {
      hover = true;
   }

   switch (command->edit.currState) {
   case EditState::IDLE:
      if (hover && mouseClicked) {
         // Make new temp command
         command->edit.delta = command->delta;
         if (isNote) { command->edit.note = command->bytes[0]; }
         command->edit.currState = EditState::DRAGGING;
      }
      break;
   case EditState::DRAGGING:
      // Update delta with mousepos
      command->edit.delta = ((mousePos.x - offset.x) / trackScale.x) - trackOffset.x;
      if (isNote) {
         float pitch = ((mousePos.y - offset.y) / trackScale.y) - trackOffset.y;
         float note = -1.0f * (((pitch + 5.0) / 10.0f) - 0x100); note += 0.5f;
         command->edit.note = std::round(note);
      }
      if (mouseReleased) {
         // finish doing stuff
         command->delta = command->edit.delta;
         if (isNote) { command->bytes[0] = command->edit.note; }
         command->edit.currState = EditState::IDLE;
      }
      break;
   default:
      break;
   }

   drawList->AddRectFilled(ImVec2(X, Y), ImVec2(X2, Y2),
      (hover || (command->edit.currState == EditState::DRAGGING)) ? red : yellow,
      5.0f, ImDrawFlags_RoundCornersAll);
}

void RenderPianoRoll(Midi64& midi, ImGuiIO& io) {
   std::string title = "Piano Roll! Track#" + std::to_string(trackID) + "###PianoRoll";
   ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.15f, 0.15f, 1.0f)); // Set window background to grey?
   ImGui::Begin(title.c_str(), NULL, ImGuiWindowFlags_NoMove);
   /* Scroll Controls */
   if (ImGui::IsWindowHovered()) {
      //ImVec2 mousePos = ImGui::GetMousePos();
      //std::cout << "X: " << mousePos.x << ", Y: " << mousePos.y << std::endl;
      
      //float value = ImGui::GetIO().MouseWheel;
      //if (value == 0) {
      //   scrollSpeed = 0;
      //}
      //else {
      //   scrollSpeed += value * 10;
      //}
      //trackScroll.y -= scrollSpeed * 2;
   }

   /* Draw tracks! */
   ImDrawList* drawList = ImGui::GetWindowDrawList();
   ImVec2 offset = ImGui::GetWindowPos();
   float winWidth = ImGui::GetWindowWidth();
   float winHeight = ImGui::GetWindowHeight();
   if (io.KeyCtrl || (trackID == -1)) {
      bool pressed = io.KeysData[ImGuiKey_RightArrow].Down || io.KeysData[ImGuiKey_LeftArrow].Down;
      if (!wasDown || (trackID == -1)) {
         u32 pTrackID = trackID;
         trackID += int(io.KeysData[ImGuiKey_RightArrow].Down);
         trackID -= int(io.KeysData[ImGuiKey_LeftArrow].Down);
         trackID = std::clamp<int>(trackID, 0, 15);
         if (pTrackID != trackID) {
            float averagePitch = midi.GetAveragePitch(trackID);
            trackOffset.y = -1 * ((0x100 - averagePitch) * 10 - 5) + (winHeight / 2) / trackScale.y;
         }
      }
      wasDown = pressed;
   }
   else if (io.KeyShift) {
      trackScale.x += 0.005f * float(io.KeysData[ImGuiKey_RightArrow].Down);
      trackScale.x -= 0.005f * float(io.KeysData[ImGuiKey_LeftArrow].Down);
      trackScale.y += 0.015f * float(io.KeysData[ImGuiKey_UpArrow].Down);
      trackScale.y -= 0.015f * float(io.KeysData[ImGuiKey_DownArrow].Down);
      trackScale.x = std::clamp<float>(trackScale.x, 0.02f, 1.0f);
      trackScale.y = std::clamp<float>(trackScale.y, 0.372f, 10.0f);
   }
   else {
      trackOffset.x -= 2000.0f * float(io.KeysData[ImGuiKey_RightArrow].Down);
      trackOffset.x += 2000.0f * float(io.KeysData[ImGuiKey_LeftArrow].Down);
      trackOffset.y += 10.0f * float(io.KeysData[ImGuiKey_UpArrow].Down);
      trackOffset.y -= 10.0f * float(io.KeysData[ImGuiKey_DownArrow].Down);
   }
   // trackOffset.y += trackScroll.y;
   trackOffset.y = std::clamp<float>(trackOffset.y, -2560.0f, 0.0f);
   trackOffset.x = trackOffset.x > 100 ? 100 : trackOffset.x;

#define DRAW_KEY(X, Y, W, H, C)  \
             drawList->AddRectFilled(ImVec2( offset.x + (X), \
                                             offset.y + (trackOffset.y + Y) * trackScale.y), \  
                                     ImVec2( offset.x + (X + W), \
                                             offset.y + (trackOffset.y + Y + H) * trackScale.y), \
                                     C, 5.0f, ImDrawFlags_None);
   auto commands = midi.GetCommands(trackID);
   volatile float delta = 0;

   //for (u16 i = 0; i < 149; i++) {
   //   DRAW_KEY(0, 2560.0f - (i * 10 * (12.f / 7.f)) - 5, 90, 10 * (12.f / 7.f), IM_COL32(0xFF, 0xFF, 0xFF, 0xFF));
   //}

   division = midi.GetDivision();
   division = division == 0 ? 480 : division;
   for (u16 i = 0; i < 256; i++) {
      float m = (i % 4) == 0 ? 2.0f : 1.0f;
      float shift = trackOffset.x * trackScale.x;
      float x = offset.x + shift + i * (division) * trackScale.x;
      drawList->AddLine(ImVec2(x, offset.y), ImVec2(x, offset.y + winHeight),
         IM_COL32(0.125 / m, 0.125 / m, 0.125 / m, 255), 0.6725f * m * trackScale.x);
   }
   for (u16 i = 0; i < 256; i++) {
      float y = offset.y + (trackOffset.y + (2560.0f - (i * 10) + 5)) * trackScale.y;
      drawList->AddLine(ImVec2(offset.x + 70, y), ImVec2(offset.x + winWidth, y),
         IM_COL32(0.125, 0.125, 0.125, 255), 0.125f * trackScale.y);
   }

   lastEffectDelta = 0;
   for (auto command : commands) {
      //delta += command->delta;
      //if ((command->status & 0xf0) == AL_MIDI_NoteOn) {
         //std::cout << "NOTE: [pitch]" + std::to_string(command->bytes[0]) << std::endl;
         //std::cout << "NOTE: [delta]" + std::to_string(command->delta) << std::endl;
         //std::cout << "NOTE: [duration]" + std::to_string(duration) << std::endl;
      RenderNote(command, offset, drawList);
         //delta += duration / 10.0f;
      //}
   }

   // Draw Piano
   bool blackKey[12] = { 0, 1, 0, 1, 0, 0, 1 ,0, 1, 0, 1, 0 };
   for (u16 i = 0; i < 256; i++) {
      if (blackKey[i % 12]) {
         DRAW_KEY(0, 2560.0f - (i * 10) - 5, 90, 10, IM_COL32(0x00, 0x00, 0x00, 0xFF));
      }
      else {
         DRAW_KEY(0, 2560.0f - (i * 10) - 5, 90, 10, IM_COL32(0xFF, 0xFF, 0xFF, 0xFF));
      }
   }

   ImGui::PopStyleColor();
   ImGui::End();
}

void SetCurrentTrack(int newTrackID) {
   trackID = newTrackID;
}
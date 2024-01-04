#include "virtualkeyboard.hpp"
#include "imgui.h"

/* Keyboard Midi State */
int pmidiKeyStates[256] = { false };
int midiKeyStates[256]  = { false };

int instrumentNum = 0;
int pInstrumentNum = 0;

int octaveBias = 0;
bool IsBlack(int key) {
   return (!((key - 1) % 7 == 0 || (key - 1) % 7 == 3) && key != 51);
}

bool IsMapped(Bank64* bank, int key) {
   if (instrumentNum >= bank->instruments.size()) {
      return false;
   }

   Inst64& inst = bank->instruments[instrumentNum];
   for (Sound64& s : inst.sounds) {
      if (s.keymap.keyMax >= key &&
         s.keymap.keyMin <= key) {
         return true;
      }
   }
   return false;
}

void UpdateKeys() {
   // White Bottom
   midiKeyStates[48 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_Z);
   midiKeyStates[50 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_X);
   midiKeyStates[52 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_C);
   midiKeyStates[53 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_V);
   midiKeyStates[55 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_B);
   midiKeyStates[57 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_N);
   midiKeyStates[59 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_M);

   // Black Bottom
   midiKeyStates[49 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_S);
   midiKeyStates[51 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_D);
   midiKeyStates[54 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_G);
   midiKeyStates[56 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_H);
   midiKeyStates[58 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_J);

   // White Top
   midiKeyStates[60 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_Q);
   midiKeyStates[62 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_W);
   midiKeyStates[64 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_E);
   midiKeyStates[65 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_R);
   midiKeyStates[67 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_T);
   midiKeyStates[69 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_Y);
   midiKeyStates[71 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_U);
   midiKeyStates[72 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_I);

   // Black Top
   midiKeyStates[61 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_2);
   midiKeyStates[63 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_3);
   midiKeyStates[66 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_5);
   midiKeyStates[68 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_6);
   midiKeyStates[70 + octaveBias * 12] = ImGui::IsKeyDown(ImGuiKey_7);
}



void RenderVirtualKeyboard(Bank64* bank) {
   ImU32 Black    = IM_COL32(0, 0, 0, 255);
   ImU32 White    = IM_COL32(255, 255, 255, 255);
   ImU32 Red      = IM_COL32(255, 0, 0, 255);
   ImU32 Grey     = IM_COL32(150, 150, 150, 255);
   ImU32 DarkGrey = IM_COL32(50, 50, 50, 255);
   ImGui::Begin("Keyboard");


   /* Update instrument value */
   pInstrumentNum = instrumentNum;
   ImGui::Text("Channel: ");
   ImGui::SameLine();
   ImGui::SliderInt("##channel", &instrumentNum, 0x00, bank->instruments.size() - 1, "0x%01X");

   /* Update Octave Bias */
   ImGui::Text("Octave Bias: ");
   ImGui::SameLine();
   ImGui::SliderInt("##octave", &octaveBias, -2, 3, "%01");

   /* Update key values */
   for (int i = 0; i < 256; i++) {
      pmidiKeyStates[i] = midiKeyStates[i];
   }
   if (ImGui::IsWindowFocused()) {
      UpdateKeys();
   }

   ImDrawList* draw_list = ImGui::GetWindowDrawList();
   ImVec2 p = ImGui::GetCursorScreenPos();
   int width = 20;
   int cur_key = 21;
   for (int key = 0; key < 52; key++) {
      ImU32 col = White;
      if (midiKeyStates[cur_key]) {
         col = Red;
      }
      if (!IsMapped(bank, cur_key)) {
         col = Grey;
      }

      draw_list->AddRectFilled(
         ImVec2(p.x + key * width, p.y),
         ImVec2(p.x + key * width + width, p.y + 120),
         col, 0, ImDrawCornerFlags_All);
      draw_list->AddRect(
         ImVec2(p.x + key * width, p.y),
         ImVec2(p.x + key * width + width, p.y + 120),
         Black, 0, ImDrawCornerFlags_All);
      cur_key++;
      if (IsBlack(key)) {
         cur_key++;
      }
   }
   cur_key = 22;
   for (int key = 0; key < 52; key++) {
      if (IsBlack(key)) {
         ImU32 col = Black;
         if (midiKeyStates[cur_key]) {
            col = Red;
         }
         if (!IsMapped(bank, cur_key)) {
            col = DarkGrey;
         }
         draw_list->AddRectFilled(
            ImVec2(p.x + key * width + width * 3 / 4, p.y),
            ImVec2(p.x + key * width + width * 5 / 4 + 1, p.y + 80),
            col, 0, ImDrawCornerFlags_All);
         draw_list->AddRect(
            ImVec2(p.x + key * width + width * 3 / 4, p.y),
            ImVec2(p.x + key * width + width * 5 / 4 + 1, p.y + 80),
            Black, 0, ImDrawCornerFlags_All);

         cur_key += 2;
      }
      else {
         cur_key++;
      }
   }
   ImGui::End();
}

bool GetInstrumentChanged() {
   return pInstrumentNum != instrumentNum;
}

int GetInstrument() {
   return instrumentNum;
}

bool GetKeyHit(int key) {
   return !pmidiKeyStates[key] && midiKeyStates[key];
}

bool GetKeyReleased(int key) {
   return pmidiKeyStates[key] && !midiKeyStates[key];
}

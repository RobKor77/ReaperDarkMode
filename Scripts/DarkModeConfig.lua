-------------------------------------------
------ WORMHOLE LABS ----------------------
------ REAPER DarkMode Configurator -------
------ version 1.0 (2026) -----------------
-------------------------------------------


local ctx = reaper.ImGui_CreateContext('RobConfigFinal')
local ini_path = reaper.GetResourcePath() .. '/UserPlugins/reaper_darkmode.ini'

local function rgb(r, g, b)
  return (r << 24) | (g << 16) | (b << 8) | 0xFF
end

local is_enabled = true
local pending_enabled = true

-- Default values
local settings = {
  {key = "TitleBarColor", label = "Window Title Bar", def =                         rgb(22, 22, 22)},
  {key = "ColorMain", label = "Inner window Background", def =                      rgb(48, 48, 48)},  
  {key = "ColorChild", label = "Main Window Background", def =                      rgb(32, 32, 32)},  
  {key = "ColorEditBackground", label = "Edit Fields", def =                        rgb(35, 35, 35)},  
  {key = "TextColor", label = "Text Color", def =                                   rgb(160, 160, 160)},  
  {key = "DisabledTextColor", label = "Disabled Text", def =                        rgb(120, 120, 120)},  
  {key = "MainWindowBorder", label = "Main Window Border", def =                    rgb(90, 90, 90)},
  {key = "BorderColor", label = "Internal Borders", def =                           rgb(70, 70, 70)},  
  {key = "GroupBoxColor", label = "GroupBox Labels", def =                          rgb(160, 160, 160)},
  {key = "HeaderBackground", label = "Table Header Background", def =               rgb(80, 80, 80)},  
  {key = "HeaderTextColor", label = "Table Header Text", def =                      rgb(240, 240, 240)},  
  {key = "TreeSelectionTextColor", label = "Selected Item Text", def =              rgb(255, 255, 255)},
  {key = "MenuBarBackground", label = "Menu Bar Background", def =                  rgb(22, 22, 22)},
  {key = "MenuBarHover", label = "Menu Bar Hover", def =                            rgb(62, 62, 62)},
  {key = "MenuTextColor", label = "Menu Bar Text", def =                            rgb(220, 220, 220)},
  {key = "MenuTextDisabled", label = "Menu Text Disabled (possibly unused)", def =  rgb(120, 120, 120)},
  {key = "TabBackground", label = "Inactive Tab Background", def =                  rgb(56, 56, 56)},
  {key = "TabSelected", label = "Active Tab Background", def =                      rgb(32, 32, 32)},
  {key = "SystemWindowsColor", label = "System Dialogs (Save/Export...)", def =     rgb(75, 75, 75)}
}

function push_dark_style()
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_WindowBg(),         0x202020FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_TitleBg(),          0x121212FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_TitleBgActive(),    0x161616FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_Text(),             0xDDDDDDFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_TextDisabled(),     0x888888FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_Header(),           0x333333FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_HeaderHovered(),    0x444444FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_HeaderActive(),     0x222222FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_Button(),           0x333333FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_ButtonHovered(),    0x444444FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_ButtonActive(),     0x222222FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_Border(),           0x555555FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_FrameBg(),          0x333333FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_FrameBgHovered(),   0x444444FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_FrameBgActive(),    0x222222FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_PopupBg(),          0x1E1E1EFF)
end

function push_light_style()
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_WindowBg(),         0xE6E6E6FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_TitleBg(),          0xDCDCDCFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_TitleBgActive(),    0xCFCFCFFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_Text(),             0x202020FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_TextDisabled(),     0x808080FF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_Header(),           0xDDDDDDFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_HeaderHovered(),    0xCCCCCCFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_HeaderActive(),     0xBBBBBBFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_Button(),           0xDDDDDDFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_ButtonHovered(),    0xCCCCCCFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_ButtonActive(),     0xBBBBBBFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_Border(),           0xAAAAAAFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_FrameBg(),          0xFFFFFFFf)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_FrameBgHovered(),   0xEEEEEEFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_FrameBgActive(),    0xDDDDDDFF)
  reaper.ImGui_PushStyleColor(ctx, reaper.ImGui_Col_PopupBg(),          0xFFFFFFFF)
end

local selected_idx = 1
local temp_color = settings[1].def
local last_selected = -1
local pending_modal = false  -- Flag za naslednji frame
local show_modal = false
local modal_message = ""

local function rgba_to_string(rgba)
  return string.format( "%d, %d, %d",(rgba >> 24) & 0xFF,(rgba >> 16) & 0xFF,(rgba >> 8) & 0xFF)
end

local function load_ini()

  local content = ""
  local file = io.open(ini_path, "r")

  if file then
    content = file:read("*all")
    file:close()

    -- Odstranitev null bytov, če C++ zapiše UTF-16
    content = content:gsub("%z", "")
  end

  -- State check
  local enabled_match = content:match("Enabled%s*=%s*(%d)")

  if enabled_match then
    is_enabled = (tonumber(enabled_match) == 1)
    pending_enabled = is_enabled
  end

  for _, item in ipairs(settings) do

    local color_val = item.def

    local r, g, b = content:match(item.key .. "%s*=%s*(%d+)%s*,%s*(%d+)%s*,%s*(%d+)")

    if r and g and b then
      color_val =
        (tonumber(r) << 24) |
        (tonumber(g) << 16) |
        (tonumber(b) << 8) |
        0xFF
    end

    item.color = color_val
    item.ini_color = color_val
  end
end

load_ini()

function loop()

  if is_enabled then
    push_dark_style()
  else
    push_light_style()
  end

  reaper.ImGui_PushStyleVar(
    ctx,
    reaper.ImGui_StyleVar_FrameRounding(),
    6.0
  )

  reaper.ImGui_PushStyleVar(ctx,reaper.ImGui_StyleVar_FrameBorderSize(),1.0)

  reaper.ImGui_SetNextWindowSize(ctx,485,445,reaper.ImGui_Cond_Always())

  local window_flags =
    reaper.ImGui_WindowFlags_NoResize() |
    reaper.ImGui_WindowFlags_NoCollapse()

  local visible, open =reaper.ImGui_Begin(ctx,'DarkMode Configurator',true,window_flags)

  if visible then

    if last_selected ~= selected_idx then
      temp_color = settings[selected_idx].color
      last_selected = selected_idx
    end

    reaper.ImGui_BeginChild(ctx, "left_pane", 235, 0, 1)

    if reaper.ImGui_CollapsingHeader(ctx, "Windows and Menus") then
      for _, i in ipairs({1, 13, 14, 15, 16}) do
        local item = settings[i]

        reaper.ImGui_ColorButton(ctx,"##c" .. item.key,item.color,0,12,12)

        reaper.ImGui_SameLine(ctx)

        if reaper.ImGui_Selectable(ctx,item.label,selected_idx == i) then
          selected_idx = i
        end
      end
    end

    if reaper.ImGui_CollapsingHeader(ctx, "Backgrounds and Borders") then
      for _, i in ipairs({19, 3, 2, 7, 8, 10, 4, 17, 18}) do
        local item = settings[i]

        reaper.ImGui_ColorButton(ctx,"##c" .. item.key,item.color,0,12,12)

        reaper.ImGui_SameLine(ctx)

        if reaper.ImGui_Selectable(
          ctx,
          item.label,
          selected_idx == i
        ) then
          selected_idx = i
        end
      end
    end

    if reaper.ImGui_CollapsingHeader(ctx, "Text & Labels") then
      for _, i in ipairs({5, 12, 11, 9}) do
        local item = settings[i]

        reaper.ImGui_ColorButton(ctx,"##c" .. item.key,item.color,0,12,12)

        reaper.ImGui_SameLine(ctx)

        if reaper.ImGui_Selectable(ctx,item.label,selected_idx == i) then
          selected_idx = i
        end
      end
    end

    reaper.ImGui_EndChild(ctx)
    reaper.ImGui_SameLine(ctx)

    reaper.ImGui_BeginGroup(ctx)

    -- DARKMODE CHECKBOX ON/OFF
    local changed_en, new_en =reaper.ImGui_Checkbox(ctx,"Enable Dark Mode",pending_enabled)

    if changed_en then
      pending_enabled = new_en
    end

    reaper.ImGui_Separator(ctx)
    reaper.ImGui_Spacing(ctx)

    local current = settings[selected_idx]

    reaper.ImGui_Text(
      ctx,
      "Editing: " .. current.label
    )

    reaper.ImGui_Separator(ctx)

    reaper.ImGui_Text(ctx, "Current:")
    reaper.ImGui_SameLine(ctx, 65)

    reaper.ImGui_ColorButton(ctx,"##old",current.color,0,35,18)
    reaper.ImGui_SameLine(ctx, 115)
    reaper.ImGui_Text(ctx, "New:")
    reaper.ImGui_SameLine(ctx, 155)
    reaper.ImGui_ColorButton(ctx,"##new",temp_color,0,35,18)

    reaper.ImGui_Spacing(ctx)

    reaper.ImGui_SetNextItemWidth(ctx, 225)

    local p_flags =
      reaper.ImGui_ColorEditFlags_NoSidePreview() |
      reaper.ImGui_ColorEditFlags_NoSmallPreview() |
      reaper.ImGui_ColorEditFlags_NoInputs() |
      reaper.ImGui_ColorEditFlags_NoLabel()

    local changed, new_temp =
      reaper.ImGui_ColorPicker4(ctx,"##picker",temp_color,p_flags)

    if changed then
      temp_color = new_temp
    end

    reaper.ImGui_Spacing(ctx)

    local r_c =(temp_color >> 24) & 0xFF
    local g_c =(temp_color >> 16) & 0xFF
    local b_c =(temp_color >> 8) & 0xFF

    reaper.ImGui_SetNextItemWidth(ctx, 69)
    local rv, r_n =
    reaper.ImGui_InputInt(ctx, "##r", r_c, 0, 0)
    reaper.ImGui_SameLine(ctx)
    reaper.ImGui_SetNextItemWidth(ctx, 69)
    local gv, g_n = reaper.ImGui_InputInt(ctx, "##g", g_c, 0, 0)

    reaper.ImGui_SameLine(ctx)
    reaper.ImGui_SetNextItemWidth(ctx, 69)
    local bv, b_n = reaper.ImGui_InputInt(ctx, "##b", b_c, 0, 0)

    if rv or gv or bv then
      temp_color =
        ((r_n or r_c) << 24) |
        ((g_n or g_c) << 16) |
        ((b_n or b_c) << 8) |
        0xFF
    end

    reaper.ImGui_Spacing(ctx)

    if reaper.ImGui_Button(ctx, 'APPLY', 69, 25) then
      settings[selected_idx].color = temp_color
    end

    reaper.ImGui_SameLine(ctx)

    if reaper.ImGui_Button(ctx, 'REVERT', 69, 25) then
      temp_color = settings[selected_idx].ini_color
      settings[selected_idx].color = settings[selected_idx].ini_color
    end

    reaper.ImGui_SameLine(ctx)

    if reaper.ImGui_Button(ctx, 'DEFAULT', 69, 25) then
      temp_color = settings[selected_idx].def
      settings[selected_idx].color = settings[selected_idx].def
    end

    reaper.ImGui_Separator(ctx)

    if reaper.ImGui_Button(ctx,'RESET ALL TO DEFAULTS',225,25) then
      for _, item in ipairs(settings) do
        item.color = item.def
      end

      temp_color = settings[selected_idx].def
    end

    if reaper.ImGui_Button(
      ctx,'SAVE & APPLY CHANGES',225,25) then

      local f = io.open(ini_path, "w")

      if f then

        -- Write pending state
        f:write("[Settings]\n")
        f:write("Enabled=" ..(pending_enabled and "1" or "0") .."\n\n")

        f:write("[Colors]\n")

        for _, item in ipairs(settings) do
          f:write(item.key .."=" ..rgba_to_string(item.color) .."\n")
          item.ini_color = item.color
        end

        f:close()

        is_enabled = pending_enabled
        
        reaper.SetExtState(
          "RobDarkMode","Update","1",false)

        reaper.TrackList_UpdateAllExternalSurfaces()
        
        -- delayed frame
        pending_modal = true
        if is_enabled then
          modal_message = "Changes will be applied!\nPlease make sure your Windows Personalization Colors are set to DARK!"
        else
          modal_message = "Changes will be applied!\nPlease make sure your Windows Personalization Colors are set to LIGHT!"
        end
      end
    end

    reaper.ImGui_EndGroup(ctx)
  end

  -- pending - wait one frame before displaying
  if pending_modal then
    show_modal = true
    pending_modal = false
  end

  -- PNon-blocking Dialog Window
if show_modal then
  reaper.ImGui_OpenPopup(ctx, "Settings Saved")
  reaper.ImGui_SetNextWindowSize(ctx, 470, 110, reaper.ImGui_Cond_Always())

  -- center popup to main window
  local main_pos_x, main_pos_y = reaper.ImGui_GetWindowPos(ctx)
  local main_w, main_h = reaper.ImGui_GetWindowSize(ctx)

  local popup_w, popup_h = 470, 110  -- enako kot SetNextWindowSize
  local center_x = main_pos_x + (main_w - popup_w) / 2
  local center_y = main_pos_y + (main_h - popup_h) / 2

  reaper.ImGui_SetNextWindowPos(ctx, center_x, center_y, reaper.ImGui_Cond_Always())

  show_modal = false
end

local modal_flags =
  reaper.ImGui_WindowFlags_NoResize() |
  reaper.ImGui_WindowFlags_NoCollapse()

if reaper.ImGui_BeginPopupModal(ctx, "Settings Saved", true, modal_flags) then
  reaper.ImGui_TextWrapped(ctx, modal_message)
  reaper.ImGui_Spacing(ctx)
  reaper.ImGui_Spacing(ctx)

  local button_width = 100
  local avail_width = reaper.ImGui_GetContentRegionAvail(ctx)
  local center_pos = (avail_width - button_width) / 2
  reaper.ImGui_SetCursorPosX(ctx, center_pos)

  if reaper.ImGui_Button(ctx, "OK##close_modal", button_width, 0) then
    reaper.ImGui_CloseCurrentPopup(ctx)
  end

  reaper.ImGui_EndPopup(ctx)
end

  reaper.ImGui_End(ctx)

  reaper.ImGui_PopStyleColor(ctx, 16)
  reaper.ImGui_PopStyleVar(ctx, 2)

  if open then
    reaper.defer(loop)
  end
end

reaper.defer(loop)

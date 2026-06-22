package com.xiaomi.settings.peripheral;

import android.content.Context;
import android.hardware.input.InputManager;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;
import android.view.InputDevice;

public class KeyboardToggleTileService extends TileService {
    
    private boolean isEnabled = true;

    @Override
    public void onStartListening() {
        super.onStartListening();
        updateTile();
    }

    @Override
    public void onClick() {
        super.onClick();
        isEnabled = !isEnabled;
        toggleKeyboardNative(isEnabled);
        updateTile();
    }

    private void toggleKeyboardNative(boolean enable) {
        InputManager inputManager = (InputManager) getSystemService(Context.INPUT_SERVICE);
        for (int id : inputManager.getInputDeviceIds()) {
            InputDevice device = inputManager.getInputDevice(id);
            if (device != null && device.getVendorId() == 5593 && device.getProductId() == 163) {
                if (enable && !device.isEnabled()) {
                    inputManager.enableInputDevice(id);
                } else if (!enable && device.isEnabled()) {
                    inputManager.disableInputDevice(id);
                }
            }
        }
    }

    private void updateTile() {
        Tile tile = getQsTile();
        if (tile == null) return;
        tile.setState(isEnabled ? Tile.STATE_ACTIVE : Tile.STATE_INACTIVE);
        tile.setSubtitle(isEnabled ? "Connected" : "OSK Restored");
        tile.updateTile();
    }
}
/*
 * Copyright (C) thanick49
 *
 * SPDX-License-Identifier: free to use idc
 */

package com.xiaomi.settings.peripheral;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.hardware.input.InputManager;
import android.os.IBinder;
import android.view.InputDevice;

public class KeyboardUtilsService extends Service {

    private static InputManager mInputManager;

    private final InputManager.InputDeviceListener mInputDeviceListener = new InputManager.InputDeviceListener() {
        @Override
        public void onInputDeviceAdded(int id) {
            enableExternalDevicesIfNeeded(id);
        }

        @Override
        public void onInputDeviceRemoved(int id) {
        
        }

        @Override
        public void onInputDeviceChanged(int id) {
            enableExternalDevicesIfNeeded(id);
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        if (mInputManager == null) {
            mInputManager = (InputManager) getSystemService(Context.INPUT_SERVICE);
        }
        if (mInputManager != null) {
            mInputManager.registerInputDeviceListener(mInputDeviceListener, null);
            setKeyboardEnabled(false);
            for (int id : mInputManager.getInputDeviceIds()) {
                enableExternalDevicesIfNeeded(id);
            }
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        if (mInputManager != null) {
            mInputManager.unregisterInputDeviceListener(mInputDeviceListener);
        }
        super.onDestroy();
    }

    private void enableExternalDevicesIfNeeded(int id) {
        if (mInputManager == null) return;
        
        InputDevice device = mInputManager.getInputDevice(id);

        if (device != null && !device.isVirtual() && !device.isEnabled()) {
            
            if (!isDeviceXiaomiKeyboard(device)) {
                int sources = device.getSources();
                boolean isKeyboard = (sources & InputDevice.SOURCE_KEYBOARD) == InputDevice.SOURCE_KEYBOARD;
                boolean isMouse = (sources & InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE;
                
                if (isKeyboard || isMouse) {
                    mInputManager.enableInputDevice(id);
                }
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    public static void setKeyboardEnabled(boolean enabled) {
        if (mInputManager == null) return;
        
        for (int id : mInputManager.getInputDeviceIds()) {
            InputDevice device = mInputManager.getInputDevice(id);
            

            if (isDeviceXiaomiKeyboard(device)) {
                if (enabled && !device.isEnabled()) {
                    mInputManager.enableInputDevice(id);
                } else if (!enabled && device.isEnabled()) {
                    mInputManager.disableInputDevice(id);
                }
            }
        }
    }

    private static boolean isDeviceXiaomiKeyboard(InputDevice device) {
        if (device == null) return false;
        return device.getVendorId() == 5593 && device.getProductId() == 163;
    }
}
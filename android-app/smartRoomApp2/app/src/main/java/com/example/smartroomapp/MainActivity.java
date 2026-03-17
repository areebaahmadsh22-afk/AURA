package com.example.smartroomapp;

import android.animation.ArgbEvaluator;
import android.animation.ValueAnimator;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.json.JSONObject;
import java.io.IOException;

import okhttp3.Call;
import okhttp3.Callback;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;

public class MainActivity extends AppCompatActivity {

    // Your current Python PC IP Address
    private static final String SERVER_URL = "http://10.2.28.42:5000/status";

    private TextView tvEmotion, tvEmoji, tvPulse, tvFan, tvBuzzer;
    private ConstraintLayout mainLayout;
    private OkHttpClient client;
    private Handler handler;
    private Runnable fetchRunnable;

    // Track the current background color for smooth animation
    private int currentColor = Color.parseColor("#2C3E50");

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Hide the top action bar for a cleaner look
        if (getSupportActionBar() != null) {
            getSupportActionBar().hide();
        }

        tvEmotion = findViewById(R.id.tvEmotion);
        tvEmoji = findViewById(R.id.tvEmoji); // NEW: The Emoji icon
        tvPulse = findViewById(R.id.tvPulse);
        tvFan = findViewById(R.id.tvFan);
        tvBuzzer = findViewById(R.id.tvBuzzer);
        mainLayout = findViewById(R.id.main_layout);

        client = new OkHttpClient();
        handler = new Handler(Looper.getMainLooper());

        fetchRunnable = new Runnable() {
            @Override
            public void run() {
                fetchRoomStatus();
                handler.postDelayed(this, 2000);
            }
        };
        handler.post(fetchRunnable);
    }

    private void fetchRoomStatus() {
        Request request = new Request.Builder().url(SERVER_URL).build();

        client.newCall(request).enqueue(new Callback() {
            @Override
            public void onFailure(Call call, IOException e) { }

            @Override
            public void onResponse(Call call, Response response) throws IOException {
                if (response.isSuccessful()) {
                    String responseData = response.body().string();
                    try {
                        JSONObject json = new JSONObject(responseData);
                        final String emotion = json.getString("emotion");
                        final int pulse = json.getInt("pulse");
                        final String fan = json.getString("fan");
                        final String ledColor = json.getString("led");
                        final String buzzer = json.getString("buzzer");

                        // Update UI on the Main Thread
                        runOnUiThread(() -> updateUI(emotion, pulse, fan, buzzer, ledColor));

                    } catch (Exception e) {
                        e.printStackTrace();
                    }
                }
            }
        });
    }

    private void updateUI(String emotion, int pulse, String fan, String buzzer, String ledColorHex) {
        tvEmotion.setText(emotion.toUpperCase());
        tvPulse.setText(pulse + " bpm");
        tvFan.setText("Fan: " + fan);
        tvBuzzer.setText(buzzer.equals("OFF") ? "Beep: OFF" : "Beep: ON");

        // Set matching Emoji based on emotion
        switch (emotion.toLowerCase()) {
            case "sad":
                tvEmoji.setText("😔");
                break;
            case "happy":
                tvEmoji.setText("😊");
                break;
            case "angry":
                tvEmoji.setText("😠");
                break;
            case "fear":
                tvEmoji.setText("😨");
                break;
            default:
                tvEmoji.setText("😐");
                break;
        }

        // FIX FOR THE INVISIBLE TEXT:
        // If the Python server sends White (#FFFFFF), we use a dark slate-grey instead
        // so our white text doesn't disappear into the background!
        int targetColor;
        if (ledColorHex.equals("#FFFFFF")) {
            targetColor = Color.parseColor("#2C3E50"); // Premium dark grey
        } else if (ledColorHex.equals("#0000FF")) {
            targetColor = Color.parseColor("#192A56"); // Deep royal blue
        } else if (ledColorHex.equals("#FFD700")) {
            targetColor = Color.parseColor("#E1B12C"); // Warm golden yellow
        } else {
            targetColor = Color.parseColor(ledColorHex);
        }

        // Smoothly animate the background color change
        if (currentColor != targetColor) {
            ValueAnimator colorAnimation = ValueAnimator.ofObject(new ArgbEvaluator(), currentColor, targetColor);
            colorAnimation.setDuration(800); // 0.8 seconds fade
            colorAnimation.addUpdateListener(animator ->
                    mainLayout.setBackgroundColor((int) animator.getAnimatedValue())
            );
            colorAnimation.start();
            currentColor = targetColor;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        handler.removeCallbacks(fetchRunnable);
    }
}
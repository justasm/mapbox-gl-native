package com.mapbox.mapboxsdk.testapp.activity.performance;

import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.FragmentTransaction;
import android.support.v7.app.ActionBar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.MenuItem;
import android.view.View;

import com.mapbox.mapboxsdk.camera.CameraPosition;
import com.mapbox.mapboxsdk.camera.CameraUpdateFactory;
import com.mapbox.mapboxsdk.constants.Style;
import com.mapbox.mapboxsdk.geometry.LatLng;
import com.mapbox.mapboxsdk.maps.MapboxMap;
import com.mapbox.mapboxsdk.maps.MapboxMapOptions;
import com.mapbox.mapboxsdk.maps.OnMapReadyCallback;
import com.mapbox.mapboxsdk.maps.SupportMapFragment;
import com.mapbox.mapboxsdk.testapp.R;
import com.mapbox.mapboxsdk.testapp.model.constants.AppConstant;

/**
 * Demonstrates jank during scroll. Demonstrates difference between MapView backed by TextureView
 * and one backed by SurfaceView.
 */
public class ScrollJankActivity extends AppCompatActivity {

    private static final int TEXTURE_VIEW_POSITION = 0;
    private static final int SURFACE_VIEW_POSITION = 1;

    private static final String POSITION_KEY = "position";

    private MapboxMap mapboxMap;

    private int position = TEXTURE_VIEW_POSITION;
    private int scrollToggle;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_scroll_jank);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        ActionBar actionBar = getSupportActionBar();
        if (actionBar != null) {
            actionBar.setDisplayHomeAsUpEnabled(true);
            actionBar.setDisplayShowHomeEnabled(true);
        }

        if (null != savedInstanceState) {
            position = savedInstanceState.getInt(POSITION_KEY, TEXTURE_VIEW_POSITION);
        }

        initSelectedFragment();

        findViewById(R.id.textureViewButton).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                position = TEXTURE_VIEW_POSITION;
                initSelectedFragment();
            }
        });
        findViewById(R.id.surfaceViewButton).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                position = SURFACE_VIEW_POSITION;
                initSelectedFragment();
            }
        });
        findViewById(R.id.animateScrollButton).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (null != mapboxMap) {
                    mapboxMap.easeCamera(
                            CameraUpdateFactory.scrollBy(scrollToggle * 1000, 0), 1000);
                    scrollToggle *= -1;
                }
            }
        });
    }

    private void initSelectedFragment() {
        mapboxMap = null;
        scrollToggle = 1;
        boolean useSurfaceView = (SURFACE_VIEW_POSITION == position);

        SupportMapFragment mapFragment =
                SupportMapFragment.newInstance(createMapOptions(useSurfaceView));

        FragmentTransaction transaction = getSupportFragmentManager().beginTransaction();
        transaction.replace(R.id.fragment_container, mapFragment);
        transaction.commit();

        mapFragment.getMapAsync(new OnMapReadyCallback() {
            @Override
            public void onMapReady(@NonNull MapboxMap mapboxMap) {
                ScrollJankActivity.this.mapboxMap = mapboxMap;
            }
        });
    }

    private static MapboxMapOptions createMapOptions(boolean useSurfaceView) {
        MapboxMapOptions options = new MapboxMapOptions();
        options.styleUrl(Style.getSatelliteStreetsStyleUrl(AppConstant.STYLE_VERSION));

        options.scrollGesturesEnabled(true);
        options.zoomGesturesEnabled(true);
        options.tiltGesturesEnabled(true);
        options.rotateGesturesEnabled(true);

        options.debugActive(false);
        options.compassEnabled(false);
        options.attributionEnabled(false);
        options.logoEnabled(false);

        options.camera(new CameraPosition.Builder()
                .target(new LatLng(48.861431, 2.334166))
                .zoom(11)
                .build());

        options.useSurfaceView(useSurfaceView);

        return options;
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(POSITION_KEY, position);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
            case android.R.id.home:
                onBackPressed();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }
}

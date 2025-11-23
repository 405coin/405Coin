package com.405Coin.qt;

import android.os.Bundle;
import android.system.ErrnoException;
import android.system.Os;

import org.qtproject.qt5.android.bindings.QtActivity;

import java.io.File;

public class 405CoinQtActivity extends QtActivity
{
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		final File 405CoinDir = new File(getFilesDir().getAbsolutePath() + "/.405Coincore");
		if (!405CoinDir.exists()) {
			405CoinDir.mkdir();
		}


		super.onCreate(savedInstanceState);
	}
}
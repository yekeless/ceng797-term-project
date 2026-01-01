import pandas as pd
import numpy as np
import scipy.stats as stats
import sys

# Dosya Adı
filename = "manual_results.csv"
scenario_name = "Simulation Results"

try:
    # 1. Sütun İsimleri
    cols = ["NodeID", "Sent", "Received", "PDR", "AvgDelay", "Throughput", "BeaconsSent", "RoleChanges"]
    
    # Dosyayı oku
    df = pd.read_csv(filename, header=None, names=cols)
    
    df["RoleChanges"] = df["RoleChanges"].fillna(0)

    # --- Node Sayısı ve Run Sayısı Tespiti ---
    zero_indices = df[df["NodeID"] == 0].index
    
    if len(zero_indices) > 1:
        NODE_COUNT = zero_indices[1] - zero_indices[0]
        num_runs = len(zero_indices)
    elif len(zero_indices) == 1:
        NODE_COUNT = len(df)
        num_runs = 1
    else:
        NODE_COUNT = len(df)
        num_runs = 1

    print(f"Tespit Edilen Node Sayısı: {NODE_COUNT}")
    print(f"Toplam Run Sayısı: {num_runs}")

    # --- METRİKLERİ HESAPLA ---
    results = {
        "Global_PDR": [],
        "Avg_Delay": [],
        "Total_Throughput": [],
        "Total_Overhead": [],
        "Avg_Stability": [] 
    }

    for i in range(num_runs):
        start_idx = zero_indices[i] if len(zero_indices) > i else 0
        end_idx = zero_indices[i+1] if (i + 1) < len(zero_indices) else len(df)
        
        chunk = df.iloc[start_idx:end_idx]
        
        # 1. Global PDR
        total_sent = chunk["Sent"].sum()
        total_recv = chunk["Received"].sum()
        pdr = (total_recv / total_sent * 100) if total_sent > 0 else 0
        results["Global_PDR"].append(pdr)
        
        # 2. Weighted Average Delay (ms Cinsinden)
        weighted_delay_sum = (chunk["AvgDelay"] * chunk["Received"]).sum()
        if total_recv > 0:
            avg_delay_sec = weighted_delay_sum / total_recv
            # Saniyeyi Milisaniyeye Çeviriyoruz (* 1000)
            results["Avg_Delay"].append(avg_delay_sec * 1000.0) 
        else:
            results["Avg_Delay"].append(0)
            
        # 3. Throughput
        total_thr = chunk["Throughput"].sum()
        results["Total_Throughput"].append(total_thr)
        
        # 4. Overhead
        total_beacon = chunk["BeaconsSent"].sum()
        results["Total_Overhead"].append(total_beacon)
        
        # 5. Stability
        avg_stability = chunk["RoleChanges"].mean()
        results["Avg_Stability"].append(avg_stability)

    # --- İSTATİSTİKLERİ YAZDIR ---
    print("\n" + "="*60)
    print(f" SONUÇ RAPORU: {scenario_name}")
    print("="*60)
    
    metrics_map = {
        "Global_PDR": "PDR (%)",
        "Avg_Delay": "Gecikme (ms)",  # Etiketi değiştirdik
        "Total_Throughput": "Throughput (bps)",
        "Total_Overhead": "Overhead (Paket Sayisi)",
        "Avg_Stability": "Stabilite (Rol Degisimi)"
    }
    
    for key, label in metrics_map.items():
        data = results[key]
        if not data: continue
            
        mean_val = np.mean(data)
        
        if num_runs > 1:
            std_err = stats.sem(data)
            h = std_err * stats.t.ppf((1 + 0.95) / 2., num_runs - 1)
            print(f"{label:25} : {mean_val:.4f} ± {h:.4f}")
        else:
            print(f"{label:25} : {mean_val:.4f} (Tek Run)")

    print("="*60)

except Exception as e:
    print("Hata:", e)

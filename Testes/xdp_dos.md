
##  📊 Guia de Execução de Testes: Cenário 1 (DoS Volumétrico)

![Tcpdump](https://img.shields.io/badge/Tcpdump-network%20analysis-orange?style=flat-square&logo=wireshark)
![Hping3](https://img.shields.io/badge/Hping3-packet%20crafting-red?style=flat-square&logo=linux)
![DDoS](https://img.shields.io/badge/DDoS%20%26%20Slow-attack%20simulation-darkred?style=flat-square&logo=apache)
![Cybersecurity](https://img.shields.io/badge/Cybersecurity-lab%20focus-blue?style=flat-square&logo=security)
![IoT](https://img.shields.io/badge/IoT-connected%20devices-lightblue?style=flat-square&logo=internetofthings)


Este guia orienta a validação do protótipo através do estabelecimento de tráfego legítimo, simulação de ataque de inundação e extração de metricas diretamente do plano de dados.

### Métricas para Ataques DoS (Volumétricos)
* **A. Taxa de Pacotes por Segundo (PPS - *Packets Per Second*)**
    * *Descrição:* Monitorização de picos súbitos no volume de pacotes recebidos pela interface de rede.
    * *Aplicação:* Identificação de saturação de infraestrutura e ataques de inundação (Flooding).
* **B. Consumo de Banda (Mbps / Gbps)**
    * *Descrição:* Volume total de tráfego de dados por unidade de tempo.
    * *Aplicação:* Análise de esgotamento do link de comunicação do gateway.

### Índice de Testes

Ao reiniciar o laboratório o Kernel do Linux é completamente zerado, isto significa que os contêineres foram parados e o programa eBPF foi apagado da memória. Caso esse seja o caso, siga essa etapas: 

```
sudo containerlab deploy -t topologia.yml --reconfigure
```
```
# executar o script para reinstalar o Mosquitto, o Python e a biblioteca do MQTT
./setup.sh
```
```
# 1. Garantir que o diretório de pins antigo seja limpo
sudo docker exec -it clab-lab-ebpf-gateway rm -f /sys/fs/bpf/xdp_monitor_test

# 2. Carregar e pinar o programa de novo
sudo docker exec -it clab-lab-ebpf-gateway bpftool prog load /lab/xdp_monitor.o /sys/fs/bpf/xdp_monitor_test type xdp

# 3. Anexar o filtro à interface eth1
sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric pinned /sys/fs/bpf/xdp_monitor_test
```
--- 
1. Preparação :

### Passo 1: Dashboard de Observação

```
# Terminal A - Iniciar dashboard - 
docker exec -it clab-lab-ebpf-gateway python3 /lab/src/dashboard.py
```

Comprova que os mecanismos de defesa não estão causando falsos positivos.

```
# Terminal B - Gera trafego legítimo MQTT no gateway
sudo docker exec -it clab-lab-ebpf-sensor python3 /src/sensor.py
```

2. Cenário A: Teste de Defesa com iptables
Neste cenário, utilizamos o Firewall nativo do Linux como Sistema de Prevenção de Intrusões (IPS) contra um ataque de Flood UDP.

   2.1. Preparar o Gateway
   Certifique-se de que o XDP está desligado e aplique as regras de mitigação do iptables:

   ```
   # 1. Desligar o eBPF/XDP da interface (se estiver ativo)
   sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric off
   
   # 2. Aplicar o script de regras do iptables 
   sudo docker exec -it clab-lab-ebpf-gateway /lab/regras_iptables.sh
   ``` 

   2.2. Iniciar o Ataque 
   
   ```
   # Terminal C: Flood UDP utilizando hping3
   docker exec -it clab-lab-ebpf-attacker hping3 --flood --udp 10.0.0.1
   ```
   
   2.3. Observação e Coleta de Métricas Iptables
   Para observar o tráfego anômalo a ser mitigado e o impacto arquitetural no kernel, execute:
   
   A. Medir o impacto na CPU e SoftIRQ (si):
   
   ```
   # Extrai o uso de processamento de rede (SoftIRQ) devido ao iptables
   sudo docker exec -it clab-lab-ebpf-gateway top -b -n 10 -d 1 | grep "%Cpu" > /lab/cpu_iptables_ddos.txt
   ```
   B. Verificar a contagem de pacotes bloqueados (Anômalos):
   
   ```
   # Lê os contadores das regras de DROP na chain INPUT
   sudo docker exec -it clab-lab-ebpf-gateway iptables -L INPUT -n -v
   ```
   
   ---

   3. Cenário B: Teste de Defesa com eBPF / XDP
   Neste cenário, substituímos o iptables pelo código eBPF acoplado no driver de rede (XDP) mapas.
   
   3.1. Preparar o Gateway
   ```
   # 1. Zerar iptables 
   sudo docker exec -it clab-lab-ebpf-gateway iptables -F
   ```
   ```
   # 2. Carregar e anexar o XDP na interface eth1 (foi desativado no Cenario A)
   sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric pinned /sys/fs/bpf/xdp_monitor_test
   ```
   
   3.2. Iniciar o Ataque
   
   ```
   # Terminal C: Flood UDP utilizando hping3
   docker exec -it clab-lab-ebpf-attacker hping3 --flood --udp 10.0.0.1
   ```
   
   3.3. Observação e Coleta de Métricas 
   
   A. Medir o impacto na CPU (Prova de Eficiência):
   
   ```
   # O valor de "si" (SoftIRQ) deve ser marginal comparado ao teste com iptables
   sudo docker exec -it clab-lab-ebpf-gateway top -b -n 10 -d 1 | grep "%Cpu" > /lab/cpu_xdp_ddos.txt
   ```
   
   B. Observar os dados extraídos pelo XDP (Leitura Nativa dos Mapas):
   
   ```
   # Ver a Volumetria Global (DDoS UDP vs Tráfego TCP na porta 1883)
   sudo docker exec -it clab-lab-ebpf-gateway bpftool map dump name proto_stats
   ```
   ```
   sudo docker exec -it clab-lab-ebpf-gateway bpftool map dump name tcp_sessions
   ```
   
   C. Visualizar via Dashboard :
   
   ```
   # monitoramento que traduz os mapas BPF em tempo real
   sudo docker exec -it clab-lab-ebpf-gateway python3 /lab/src/dashboard.py
   ```







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

### Deploy do laboratório

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
### Passo 1: Dashboard de Observação

```
# Terminal A - Iniciar dashboard e mantenha ligado durante todo laboratório.
docker exec -it clab-lab-ebpf-gateway python3 /lab/src/dashboard.py
```

Comprova que os mecanismos de defesa não estão causando falsos positivos.
Enquanto sensor legítimo envia pacotes não há alteração significativa de PPS, Banda ou CPU.

```
# Terminal B - Gera trafego legítimo MQTT no gateway
sudo docker exec -it clab-lab-ebpf-sensor python3 /src/sensor.py
```

### Passo 2: Observação com iptables
Neste cenário, utilizamos o Firewall nativo do Linux como Sistema de Prevenção de Intrusões (IPS) contra um ataque de Flood UDP.

   2.1. Preparar o Gateway
   Certifique-se de que o XDP está desligado e aplique as regras de mitigação do iptables:

   ```
   sudo docker exec -it clab-lab-ebpf-gateway apt-get update
   sudo docker exec -it clab-lab-ebpf-gateway apt-get install -y iptables
   ```

   ```
   # Desligar o eBPF/XDP da interface (se estiver ativo)
   sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric off
   
   # Aplicar o script de regras do iptables - só aceita 10 pacotes UDP por segundo
   sudo docker exec -it clab-lab-ebpf-gateway /lab/regras_iptables.sh
   ``` 

   2.2. Iniciar o Ataque 
   
   ```
   # Terminal C: Flood UDP utilizando hping3
   docker exec -it clab-lab-ebpf-atacante hping3 --flood --udp 10.0.0.1
   ```
   
   2.3. Observação e Coleta de Métricas Iptables
   Para observar o tráfego anômalo, execute:
   
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

   ### Passo 3: Observação com eBPF / XDP
   Substituir o iptables pelo código eBPF .
   
   3.1. Preparar o Gateway
   ```
   #  Zerar iptables 
   sudo docker exec -it clab-lab-ebpf-gateway iptables -F
   ```
   ```
   #  Carregar e anexar o XDP na interface eth1 (foi desativado no item 2.1)
   sudo docker exec -it clab-lab-ebpf-gateway ip link set dev eth1 xdpgeneric pinned /sys/fs/bpf/xdp_monitor_test
   ```
   
   3.2. Iniciar o Ataque
   
   ```
   # Terminal C: Flood UDP utilizando hping3
   docker exec -it clab-lab-ebpf-atacante hping3 --flood --udp 10.0.0.1
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
---
# analisar antes

1. No Contentor do Sensor (Simular sinal Wi-Fi com perdas e latência)
Para simular que o sensor está numa rede Wi-Fi com uma latência média de 30ms, uma variação de ±5ms (jitter) e uma perda aleatória de 1% dos pacotes:

```
sudo docker exec -it clab-lab-ebpf-sensor tc qdisc add dev eth1 root netem delay 30ms 5ms loss 1%
```
2. No Contentor do Atacante (Simular um atacante com ligação Wi-Fi instável)
Se quiser simular que o atacante está mais distante do ponto de acesso, enfrentando maior perda de pacotes (ex: 5%) e maior latência (ex: 50ms):

```
sudo docker exec -it clab-lab-ebpf-atacante tc qdisc add dev eth1 root netem delay 50ms 15ms loss 5%
```
3. Como remover a emulação (Voltar ao estado original de cabo perfeito)
Após concluir os testes, pode remover as restrições a qualquer momento limpando a fila (qdisc) da interface:

```
sudo docker exec -it clab-lab-ebpf-sensor tc qdisc del dev eth1 root
sudo docker exec -it clab-lab-ebpf-atacante tc qdisc del dev eth1 root
```






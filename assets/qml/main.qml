/*
 * Copyright (c) 2016-2017 Theodore Opeiko
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import bb.cascades 1.4

Page {
    Container {
        background: Color.create("#fffdf6e3")

        layout: GridLayout {}
        
        Container {
            verticalAlignment: VerticalAlignment.Center
            horizontalAlignment: HorizontalAlignment.Center
            
            Label {
                id: noteLabel
                objectName: "noteLabel"
                text: ""
                verticalAlignment: VerticalAlignment.Center
                horizontalAlignment: HorizontalAlignment.Center
                
                textStyle {
                    base: SystemDefaults.TextStyles.BigText
                    color: Color.create("#ff657b83")
                    textAlign: TextAlign.Center
                }
            }
            
            Label {
                id: tuneCentsOffsetLabel
                objectName: "tuneCentsOffsetLabel"
                text: ""
                verticalAlignment: VerticalAlignment.Center
                horizontalAlignment: HorizontalAlignment.Center
                
                textStyle {
                    base: SystemDefaults.TextStyles.BodyText
                    color: Color.create("#ff657b83")
                    textAlign: TextAlign.Center
                }
            }
            
            Container {
                horizontalAlignment: HorizontalAlignment.Center
                verticalAlignment: VerticalAlignment.Center 
                
                Label {
                    id: tuneOffsetLabel
                    objectName: "tuneOffsetLabel"
                    text: "<span>n</span> <span>n</span> <span>n</span> <span>n</span> <span>n</span>"
                    textFormat: TextFormat.Html
                    
                    textStyle {
                        base: SystemDefaults.TextStyles.PrimaryText
                        fontFamily: "Webdings"
                        color: Color.create("#ffeee8d5")
                    } 
                }
                
            }
        }    
    }
}
